#include "KVClustering.h"
#include <cppcms/http_response.h>
#include <cppcms/http_request.h>
#include <cppcms/url_dispatcher.h>
#include <cppcms/url_mapper.h>
#include <cppcms/json.h>
#include <cppcms/util.h>
#include <liboscar/KVStats.h>

namespace oscar_web {

KVClustering::KVClustering(cppcms::service& srv, const CompletionFileDataPtr& dataPtr):
cppcms::application(srv),
m_dataPtr(dataPtr)
{
	dispatcher().assign("/get", &KVClustering::get, this);
	mapper().assign("get","/get");
}

KVClustering::~KVClustering() {}

void KVClustering::get() {
	typedef sserialize::Static::spatial::GeoHierarchy GeoHierarchy;
	typedef liboscar::Static::OsmKeyValueObjectStore OsmKeyValueObjectStore;
	
	sserialize::TimeMeasurer ttm;
	ttm.begin();

	const auto & store = m_dataPtr->completer->store();
	const auto & gh = store.geoHierarchy();
	
	response().set_content_header("text/json");
	
	//params
	std::string cqs = request().get("q");
	std::string regionFilter = request().get("rf");
	std::string format = request().get("format");
	std::string queryId = request().get("queryId");

	sserialize::CellQueryResult cqr;
	sserialize::spatial::GeoHierarchySubGraph sg;
	
	if (m_dataPtr->ghSubSetCreators.count(regionFilter)) {
		sg = m_dataPtr->ghSubSetCreators.at(regionFilter);
	}
	else {
		sg = m_dataPtr->completer->ghsg();
	}
	cqr = m_dataPtr->completer->cqrComplete(cqs, sg, m_dataPtr->treedCQR, m_dataPtr->treedCQRThreads);

	auto items = cqr.flaten(m_dataPtr->treedCQRThreads);
	auto stats = liboscar::KVStats(store).stats(items, m_dataPtr->treedCQRThreads);

	std::ostream & out = response().out();
	sserialize::JsonEscaper je;
	
	auto topkeys = stats.topk(5, [](auto a, auto b) -> bool { return a.count < b.count; });
	out << "{\"kvclustering\":";
	char sep = '[';
	for(uint32_t keyId : topkeys){
		const auto & ki = stats.key(keyId);
		auto topvalues = ki.topk(5, [](auto a, auto b) -> bool { return a.count < b.count; });
		
		out << sep;
		out << "{\"name\":\"" << je.escape(store.keyStringTable().at(ki.keyId))
			<< "\",\"count\":" << ki.count
			<< ",\"clValues\":";
		sep = '[';
		for(uint32_t p : topvalues) {
			const liboscar::KVStats::ValueInfo & vi = ki.values.at(p);
			out << sep;
			sep = ',';
			out << "{\"name\":\"" << je.escape(store.valueStringTable().at(vi.valueId))
				<< "\",\"count\":" << vi.count
				<< '}';
		}
		out << "]}";
	}
	out << "],\"queryId\":" << queryId << '}';

	ttm.end();
	writeLogStats("get", cqs, ttm, cqr.cellCount(), items.size());
}

void KVClustering::writeLogStats(const std::string& fn, const std::string& query, const sserialize::TimeMeasurer& tm, uint32_t cqrSize, uint32_t idxSize) {
	*(m_dataPtr->log) << "KVClustering::" << fn << ": t=" << tm.beginTime() << "s, rip=" << request().remote_addr() << ", q=[" << query << "], rs=" << cqrSize <<  " is=" << idxSize << ", ct=" << tm.elapsedMilliSeconds() << "ms" << std::endl;
}




}//end namespace oscar_web
