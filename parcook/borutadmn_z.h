/* 
  SZARP: SCADA software 

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
/*
 * 
 * Darek Marcinkiewicz <reksio@newterm.pl>
 * 
 */

#ifndef __BORUTADMN_H_
#define __BORUTADMN_H_

/* Boruta is a meta-daemon, a generic daamon that does not implement any protocol.
 * Instead it relies on its modules - drivers to implement particular protocols.
 * Boruta is able to run any number of drivers within one process. Boruta's
 * drivers can be either clients or servers that can communicate over one of two
 * mediums: serial line or tcp protocol. 
 * Boruta itself performs following tasks:
 *  1. Manages connections 
 *  2. Schedules client drivers.
 *
 * Connections management boils down to estalibshing tcp connections (in case of
 * tcp client drivers), accepting new connections (for tcp server drivers), 
 * opening/configuring serial port (for serial connections).
 *
 * Scheduling is performed for client drivers that use the same connection (the
 * same serial line or tcp address-port pair). Client drivers are scheduled in
 * turn and each deliver is supposed to notify boruta when it's done with 
 * enquiring its peer, so that boruta can pass connection to next driver - this is 
 * kind of cooperative multitasking with respect to connections utilization.
 *
 * All drivers are supposed to perform no blocking calls, I/O
 * and timeout handling have to be perfomed through libevent API.
 *
 */

#include <netinet/in.h>
#include <event.h>
#include <evdns.h>
#include "zmqhandler.h"
#include "dmncfg.h"
#include "custom_assert.h"
#include "baseconn.h"
#include "tcpconn.h"

class driver_iface {
public:
	virtual ~driver_iface() {}
	virtual int configure(TUnit* unit, size_t read, size_t send, const SerialPortConfiguration&) = 0;
	virtual void finished_cycle() = 0;
	virtual void starting_new_cycle() = 0;
};

class bc_driver: public ConnectionListener, public driver_iface {
protected:
	BaseConnection* m_connection = nullptr;

public:
	bc_driver(BaseConnection* conn): m_connection(conn) {
		conn->AddListener(this);
	}
};

class boruta_daemon;

class bc_manager {
public:
	std::set<driver_iface*> conns;

	boruta_daemon* m_boruta;
	bc_manager(boruta_daemon* boruta): m_boruta(boruta) {}

	int configure(TUnit *unit, size_t read, size_t send);

	void finished_cycle() { for (auto cp: conns) { cp->finished_cycle(); } }
	void starting_new_cycle() { for (auto cp: conns) { cp->starting_new_cycle(); } }
};

struct boruta_logger {
	std::string header;
	boruta_logger(const std::string& _header): header(_header) {}

	void log(int level, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));
	void vlog(int level, const char * fmt, va_list fmt_args);
};

using slog = std::shared_ptr<boruta_logger>;

class boruta_daemon;

class BaseServerConnectionHandler: public BaseConnection {
public:
	using BaseConnection::BaseConnection;
	virtual void CloseConnection(const BaseConnection*) = 0;

	void WriteData(const void* data, size_t size) override {
		ASSERT(!"This should not have been called!");
		throw TcpConnectionException("Cannot write on server socket");
	}

	void SetConfiguration(const SerialPortConfiguration& serial_conf) override {}
};


// implements handling established connections
// TODO: implement
// retry_gap = unit->getAttribute("extra:connection-retry-gap", 4);
// establishment_timeout = unit->getAttribute("extra:connection-establishment-timeout", 10);
class TcpServerConnection: public BaseConnection {
	BaseServerConnectionHandler* handler;
	struct bufferevent *m_bufferevent;
	int m_fd;

public:
	TcpServerConnection(BaseServerConnectionHandler* _handler, struct event_base *ev_base): BaseConnection(ev_base), handler(_handler) {}

	~TcpServerConnection();

	void InitConnection(int fd);

	void Open() override;
	void Close() override;

	/** Returns true if connection is ready for communication */
	bool Ready() const override;
	void ReadData(struct bufferevent *bufev);
	void WriteData(const void* data, size_t size) override;
	/** Set line configuration for an already open port */
	void SetConfiguration(const SerialPortConfiguration& serial_conf) override;

	static void ErrorCallback(struct bufferevent *bufev, short event, void* conn)
	{
		if (event & BEV_EVENT_CONNECTED)
			return;

		reinterpret_cast<TcpServerConnection*>(conn)->Error(event);
	}

	static void ReadDataCallback(struct bufferevent *bufev, void* conn) {
		reinterpret_cast<TcpServerConnection*>(conn)->ReadData(bufev);
	}
};

// implements handling incoming connections
class TcpServerConnectionHandler: public BaseServerConnectionHandler, public ConnectionListener {
	int m_fd = -1;

	struct event _event;
	struct sockaddr_in m_addr;

	std::set<unsigned long> m_allowed_ips;

	std::list<std::unique_ptr<BaseConnection>> m_connections;

public:
	using BaseServerConnectionHandler::BaseServerConnectionHandler;

	void Init(TUnit* unit);

private:
	void ConfigureAllowedIps(TUnit* unit);

private:
	// socket handling
	int start_listening();

	bool ip_is_allowed(struct sockaddr_in in_s_addr);
	boost::optional<int> accept_socket(int in_fd);

	void AcceptConnection(int _fd);

	static void connection_accepted_cb(int _fd, short event, void* _handler) {
		reinterpret_cast<TcpServerConnectionHandler*>(_handler)->AcceptConnection(_fd);
	}

public:
	// Connection Listener implementation
	void OpenFinished(const BaseConnection *conn) override {}
	void ReadData(const BaseConnection *conn, const std::vector<unsigned char>& data) override;
	void ReadError(const BaseConnection *conn, short int event) override;
	void SetConfigurationFinished(const BaseConnection *conn) override {}
	void CloseConnection(const BaseConnection* conn) override;

	// BaseConnection implementation
	void Open() override;
	void Close() override;
	bool Ready() const override;
};

struct conn_factory {
	enum class mode {client, server};
	static mode get_mode(TUnit* unit) {
		std::string mode_opt = unit->getAttribute<std::string>("extra:mode", "client");
		if (mode_opt == "client") {
			return mode::client;
		} else if (mode_opt == "server") {
			return mode::server;
		} else {
			throw std::runtime_error("Invalid mode option (can be client or server)");
		}
	}

	enum class medium {tcp, serial};
	static medium get_medium(TUnit* unit) {
		std::string medium_opt = unit->getAttribute<std::string>("extra:medium", "tcp");
		if (medium_opt == "tcp") {
			return medium::tcp;
		} else if (medium_opt == "serial") {
			return medium::serial;
		} else {
			throw std::runtime_error("Invalid medium option (can be tcp or serial)");
		}
	}
};

class boruta_daemon {
	bc_manager m_drivers_manager;

	zmq::context_t m_zmq_ctx;

	DaemonConfig* m_cfg;
	zmqhandler * m_zmq;
	struct timeval m_cycle;

	struct event_base* m_event_base;
	struct evdns_base* m_evdns_base;
	struct event m_timer;
	struct event m_subsock_event;

	int configure_ipc();
	int configure_events();
	int configure_units();
	int configure_cycle_freq();
public:
	boruta_daemon();
	struct event_base* get_event_base();	
	struct evdns_base* get_evdns_base();	
	zmqhandler* get_zmq();
	int configure(int *argc, char *argv[]);
	void go();
	static void cycle_timer_callback(int fd, short event, void* daemon);
	static void subscribe_callback(int fd, short event, void* daemon);
};

driver_iface* create_bc_modbus_tcp_server(TcpServerConnectionHandler* conn, boruta_daemon*, slog);
driver_iface* create_bc_modbus_serial_server(BaseConnection* conn, boruta_daemon*, slog);
driver_iface* create_bc_modbus_serial_client(BaseConnection* conn, boruta_daemon*, slog);
driver_iface* create_bc_modbus_tcp_client(TcpConnection* conn, boruta_daemon*, slog);
driver_iface* create_fc_serial_client(BaseConnection* conn, boruta_daemon*, slog);

void dolog(int level, const char * fmt, ...)
	__attribute__ ((format (printf, 2, 3)));

namespace ascii {
	int from_ascii(unsigned char c1, unsigned char c2, unsigned char &c) ;
	void to_ascii(unsigned char c, unsigned char& c1, unsigned char &c2) ;
}

#endif
