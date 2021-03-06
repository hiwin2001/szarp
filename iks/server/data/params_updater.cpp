#include "params_updater.h"
#include "sz4/base.h"

#include <chrono>
#include <memory>
#include <functional>

namespace p = std::placeholders;

#include <liblog.h>

#include "global_service.h"

ParamsUpdater::ParamsUpdater( Params& params )
	: params(params)
{
}

ParamsUpdater::~ParamsUpdater()
{
}

void ParamsUpdater::set_data_feeder( SzbaseWrapper* data_feeder_ )
{
	data_feeder = data_feeder_;
}

ParamsUpdater::Subscription ParamsUpdater::subscribe_param(
		const std::string& name ,
		boost::optional<ProbeType> pt ,
		bool update )
{
	Subscription sub;

	auto key = SubKey( name );

	SubParPtr ptr;
	if ( !( ptr = subscribed_params[ key ].lock() ) ) {
		ptr = std::make_shared< SubPar >( name, pt, this );

		if ( !ptr->start_sub() ) {
			subscribed_params.erase( key );
			return sub;
		}

		subscribed_params.insert(std::make_pair( key,
												 SubParWeakPtr( ptr ) ) );
	}

	if( update )
		ptr->update_param();

	sub.insert(ptr);
	return sub;
}

std::string ParamsUpdater::add_param( const std::string& param
									, const std::string& base
									, const std::string& formula
									, const std::string& token
									, const std::string& type
									, int prec
									, unsigned start_time)

{
	return data_feeder->add_param( param , base , formula , token, type , prec , start_time );
}

void ParamsUpdater::remove_param( const std::string& base , const std::string& param )
{
	return data_feeder->remove_param( base , param );
}

ParamsUpdater::Subscription::Subscription()
{
}

void ParamsUpdater::Subscription::insert( const ParamsUpdater::Subscription& sub )
{
	subset.insert( sub.subset.begin() , sub.subset.end() );
}

void ParamsUpdater::Subscription::insert( const ParamsUpdater::SubParPtr& sub )
{
	subset.insert( sub );
}

ParamsUpdater::SubPar::SubPar( const std::string& pname , boost::optional<ProbeType> pt , ParamsUpdater* parent )
											   : pname( pname ) , pt( pt ) , parent( parent )
{
}

bool ParamsUpdater::SubPar::start_sub() {
	SubParWeakPtr ptr(shared_from_this());

	auto callback_lambda = [ptr] () {
		GlobalService::get_service().post( std::bind( callback, ptr ) );
	};

	try {
		token = parent->data_feeder->register_observer(pname, pt, callback_lambda);
		return true;
	} catch( szbase_error& e ) {
		sz_log( 0, "Szbase error while registering observer : %s", e.what() );
		return false;
	}
}

ParamsUpdater::SubPar::~SubPar() {
	parent->subscribed_params.erase( ParamsUpdater::SubKey( pname ) );
}

void ParamsUpdater::SubPar::update_param() {
	using std::chrono::system_clock;
	using namespace boost::posix_time;

	if( pt ) {
		parent->params.param_value_changed(
				pname ,
				parent->data_feeder->get_updated_value(pname, *pt) ,
				*pt );
	} else {
		parent->params.param_changed( pname );
	}
}

void ParamsUpdater::SubPar::callback(SubParWeakPtr ptr) {
		if (auto p = ptr.lock())
				p->update_param();
}

