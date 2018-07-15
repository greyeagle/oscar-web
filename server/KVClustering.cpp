#include "KVClustering.h"
#include <cppcms/http_response.h>
#include <cppcms/http_request.h>
#include <cppcms/url_dispatcher.h>
#include <cppcms/url_mapper.h>
#include <cppcms/json.h>
#include <cppcms/util.h>
#include <sserialize/mt/ThreadPool.h>
#include <sserialize/containers/CFLArray.h>

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

	auto items = cqr.flaten(m_dataPtr->treedCQRThreads).toVector();

	std::ostream & out = response().out();
	
	struct Data {
		std::unordered_map<std::pair<uint32_t, uint32_t>, uint32_t> keyValueCount;
		Data() {}
		Data(const Data & other) = default;
		Data(Data && other) : keyValueCount(std::move(other.keyValueCount)) {}
		Data & operator=(Data && other) {
			keyValueCount = std::move(other.keyValueCount);
			return *this;
		}
		void update(const OsmKeyValueObjectStore::KVItemBase & item) {
			for (uint32_t i(0), s(item.size()); i < s; ++i) {
				uint32_t key = item.keyId(i);
				uint32_t value = item.valueId(i);
				++(keyValueCount[std::make_pair(key, value)]); //init to 0 if not existent
			}
		}
		static Data merge(Data && first, Data && second) {
			if (first.keyValueCount.size() < second.keyValueCount.size()) {
				return merge(std::move(second), std::move(first));
			}
			if (!second.keyValueCount.size()) {
				return std::move(first);
			}
			
			for(const auto & x : second.keyValueCount) {
				first.keyValueCount[x.first] += x.second; 
			}
			second.keyValueCount.clear();
			return std::move(first);
		}
	};
	
	struct ValueInfo {
		ValueInfo() : count(0), valueId(std::numeric_limits<uint32_t>::max()) {}
		uint32_t count;
		uint32_t valueId;
	};
	
	struct KeyInfo {
		KeyInfo() : count(0), values(sserialize::CFLArray<std::vector<ValueInfo>>::DeferContainerAssignment{}) {}
		uint32_t count;
		sserialize::CFLArray< std::vector<ValueInfo> > values;
	};
	
	
	struct State {
		const OsmKeyValueObjectStore & store;
		const std::vector<uint32_t> & items;
		std::atomic<std::size_t> pos{0};
		
		std::mutex lock;
		std::vector<Data> d;
		
		State(const OsmKeyValueObjectStore & store, const std::vector<uint32_t> & items) :
		store(store),
		items(items)
		{}
	} state(store, items);

	struct Worker {
		Data d;
		State * state;
		Worker(State * state) : state(state) {}
		Worker(const Worker & other) : state(other.state) {}
		void operator()() {
			while (true) {
				std::size_t p = state->pos.fetch_add(1, std::memory_order_relaxed);
				if (p >= state->items.size()) {
					break;
				}
				uint32_t itemId = state->items[p];
				d.update( state->store.kvBaseItem(itemId) );
			}
			flush();
		}
		void flush() {
			std::unique_lock<std::mutex> lck(state->lock, std::defer_lock);
			while(true) {
				lck.lock();
				state->d.emplace_back(std::move(d));
				if (state->d.size() < 2) {
					break;
				}
				Data first = std::move(state->d.back());
				state->d.pop_back();
				Data second = std::move(state->d.back());
				state->d.pop_back();
				lck.unlock();
				this->d = Data::merge(std::move(first), std::move(second));
			}
		}
	};
	
	sserialize::ThreadPool::execute(Worker(&state), m_dataPtr->treedCQRThreads, sserialize::ThreadPool::CopyTaskTag());
	
	//calculate KeyInfo
	auto & keyValueCount = state.d.front().keyValueCount;
	std::vector<ValueInfo> valueStore(keyValueCount.size());
	std::unordered_map<uint32_t, KeyInfo> keyInfo;
	{
		//get the keys and the number of values per key
		for(const auto & x : keyValueCount) {
			const std::pair<uint32_t, uint32_t> & kv = x.first;
			const uint32_t & kvcount = x.second;
			KeyInfo & ki = keyInfo[kv.first];
			ki.count += kvcount;
			ki.values.resize(ki.values.size()+1);
		}
		//init store
		uint64_t offset = 0;
		for(auto & x : keyInfo) {
			KeyInfo & ki = x.second;
			ki.values.reposition(offset);
			ki.values.rebind(&valueStore);
			offset += ki.values.size();
			ki.values.resize(0);
		}
		//copy the values
		for(const auto & x : keyValueCount) {
			const std::pair<uint32_t, uint32_t> & kv = x.first;
			KeyInfo & ki = keyInfo[kv.first];
			ki.values.resize(ki.values.size()+1);
			ValueInfo & vi = ki.values.back();
			vi.count = x.second;
			vi.valueId = kv.second;
		}
	}
	

	out << "{\"kvclustering\":[";
	bool first0 = true;
	for(const auto & x : keyInfo){
        uint32_t keyId = x.first;
        const KeyInfo & ki = x.second;
        if (ki.count > items.size()*0.1f && ki.count > 1) {
            if (!first0) out << ",";
            first0 = false;
            out << "{";
            out << "\"name\":" << '"' << escapeJsonString(store.keyStringTable().at(keyId)) << '"' << ',' << " \"count\" : " << ki.count << ","
                << "\"clValues\" :" << "[";
            std::int32_t others = 0;

            bool first = true;

            for(const ValueInfo & vi: ki.values){
                if(vi.count > ki.count*0.1f){
                    if (!first) out << ",";
                    first = false;
                    out << R"({"name":")" << escapeJsonString(store.valueStringTable().at(vi.valueId)) << '"' << "," << "\"count\":" << vi.count << "}";
                } else {
                    others += vi.count;
                }
            }
            if(others > 0){
                if (!first) out << ",";
                out << R"({"name":")" << "others" << '"' << "," << "\"count\":" << others << "}";
            }
            out << "]}";
        }
	}

    out << "], \"queryId\":" + queryId + "}";

	ttm.end();
	writeLogStats("get", cqs, ttm, cqr.cellCount(), items.size());
}


	//ecapes strings for json, source: https://stackoverflow.com/questions/7724448/simple-json-string-escape-for-c/33799784#33799784

	std::string KVClustering::escapeJsonString(const std::string& input) {
		std::ostringstream ss;
		for (auto iter = input.cbegin(); iter != input.cend(); iter++) {
			//C++98/03:
			//for (std::string::const_iterator iter = input.begin(); iter != input.end(); iter++) {
			switch (*iter) {
				case '\\': ss << "\\\\"; break;
				case '"': ss << "\\\""; break;
				case '/': ss << "\\/"; break;
				case '\b': ss << "\\b"; break;
				case '\f': ss << "\\f"; break;
				case '\n': ss << "\\n"; break;
				case '\r': ss << "\\r"; break;
				case '\t': ss << "\\t"; break;
				default: ss << *iter; break;
			}
		}
		return ss.str();
	}

void KVClustering::writeLogStats(const std::string& fn, const std::string& query, const sserialize::TimeMeasurer& tm, uint32_t cqrSize, uint32_t idxSize) {
	*(m_dataPtr->log) << "KVClustering::" << fn << ": t=" << tm.beginTime() << "s, rip=" << request().remote_addr() << ", q=[" << query << "], rs=" << cqrSize <<  " is=" << idxSize << ", ct=" << tm.elapsedMilliSeconds() << "ms" << std::endl;
}




}//end namespace oscar_web
