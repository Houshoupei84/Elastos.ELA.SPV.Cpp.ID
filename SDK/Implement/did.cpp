//
// Created by hp on 2018/6/16.
//
#include "did.h"

#include <boost/filesystem.hpp>
#include <SDK/Common/ErrorChecker.h>
#include "didManager.h"
#include "SDK/Common/Log.h"
#include "SDK/Wrapper/Key.h"

namespace fs = boost::filesystem;

using namespace Elastos::ElaWallet;

namespace Elastos {
	namespace DID {

		CDid::CDid(CDidManager* didManager, const std::string &id) {

			Log::getLogger()->info("CDid::CDid  begin didNameStr didManager {:p} id{} ", (void*) didManager, id);
			ErrorChecker::condition(didManager == nullptr, Error::InvalidArgument, "DID manager is null");
			ErrorChecker::argumentNotEmpty(id, "ID");

			_didManger = didManager;
			_didNameStr = id;
			Log::getLogger()->info("CDid::CDid  end didNameStr didManager {:p} id{} ", (void*) didManager, id);
		}

		CDid::~CDid(){

		}

		std::string CDid::GetDIDName(){
			Log::getLogger()->info("CDid::GetDIDName   didNameStr didManager _didNameStr{} ", _didNameStr);
			return _didNameStr;
		}

		void CDid::CheckInit() const {
			//ParamChecker::checkNotEmpty(_passWord);
		}

		void CDid::DelValue(
			const std::string &path,
			uint32_t blockHeight ){

			Log::getLogger()->info("CDid::DelValue  begin path{}  blockHeight{}", path, blockHeight);

			CheckInit();
			ErrorChecker::argumentNotEmpty(path, "Path " + path);

			_didManger->_idCache->Delete(_didNameStr, path, blockHeight);

			Log::getLogger()->info("CDid::DelValue  end path{}  blockHeight{}", path, blockHeight);

		}

		void CDid::setValue(
					  const std::string &path,
					  const nlohmann::json &value,
					  uint32_t blockHeight){

			CheckInit();
			ErrorChecker::argumentNotEmpty(path, "Path " + path);

			Log::getLogger()->info("CDid::SetValue begin  path{}  value{} blockHeight{}", path, value.dump(), blockHeight);

			_didManger->_idCache->Put(_didNameStr,  path, blockHeight , value);
			Log::getLogger()->info("CDid::SetValue end  path{}  value{} blockHeight{}", path, value.dump(), blockHeight);

		}

		void CDid::SetValue(
			const std::string &keyPath ,
			const nlohmann::json &valueJson){

			Log::getLogger()->info("CDid::SetValue begin  keyPath{}  valueJson{}", keyPath, valueJson.dump());

			CheckInit();
			ErrorChecker::argumentNotEmpty(keyPath, "Key path " + keyPath);
			//检查是否json ???


			_didManger->_idCache->Put(_didNameStr,  keyPath, valueJson);
			Log::getLogger()->info("CDid::SetValue end  keyPath{}  valueJson{}", keyPath, valueJson.dump());
		}

		nlohmann::json CDid::GetValue(
			const std::string &path) const {

			Log::getLogger()->info("CDid::GetValue  begin path{} ", path);
			CheckInit();
			ErrorChecker::argumentNotEmpty(path, "Path " + path);

			nlohmann::json jsonGet;
			jsonGet =_didManger-> _idCache->Get(_didNameStr, path);
			if (jsonGet.empty()) {
				return nlohmann::json();
			}

			nlohmann::json lastIdValue;
			uint32_t blockHeight = 0;
			for (nlohmann::json::iterator it = jsonGet.begin(); it != jsonGet.end(); it++) {
				uint32_t getBlockHeight = (uint32_t)std::stoi(it.key(), nullptr, 10);
				if (getBlockHeight > blockHeight) {
					lastIdValue[it.key()] = it.value();
					blockHeight = getBlockHeight;
				}
			}

			Log::getLogger()->info("CDid::GetValue end  path{}  valueJson{}"
				, path, lastIdValue.dump());
			return lastIdValue;

		}

		nlohmann::json CDid::GetHistoryValue(
			const std::string &keyPath) const {

			Log::getLogger()->info("CDid::GetHistoryValue begin  keyPath{} ", keyPath);
			CheckInit();
			ErrorChecker::argumentNotEmpty(keyPath, "Key path " + keyPath);

			nlohmann::json jsonRet = _didManger->_idCache->Get(_didNameStr, keyPath);

			Log::getLogger()->info("CDid::GetHistoryValue end  keyPath{} jsonRet{} "
						  , keyPath , jsonRet.dump());
			return jsonRet;
		}

		//start下标第一个为0
		nlohmann::json CDid::GetAllKeys(
			uint32_t start ,
			uint32_t count) const {

			Log::getLogger()->info("CDid::GetAllKeys  begin start{}  count{}", start, count);

			CheckInit();

			nlohmann::json keysJson;

			if(count == 0){

				throw std::invalid_argument("count mut big than 0");
			}

			uint32_t addCount = 0;
			nlohmann::json jsonGet;
			jsonGet =_didManger-> _idCache->Get(_didNameStr);

			if(start >= jsonGet.size()){

				throw std::invalid_argument("start >= jsonGet.size()");
			}

			nlohmann::json::const_iterator it = jsonGet.begin();
			std::advance (it,start);
			for (; it != jsonGet.end(); it++) {
				keysJson.push_back(it.key());
				addCount++;

				if(addCount >= count){
					break;
				}
			}

			Log::getLogger()->info("CDid::GetAllKeys  end start{}  count{} keysJson{}"
						  , start, count, keysJson.dump());
			return keysJson;
		}

		nlohmann::json CDid::GenerateProgram(
			const std::string &message,
			const std::string &password){

			Log::getLogger()->info("CDid::GenerateProgram  begin lalala message{}  password{}", message, password);

			nlohmann::json jsonRet = _didManger->_iidAgent->GenerateProgram(_didNameStr, message, password);

			Log::getLogger()->info("CDid::GenerateProgram  end message{}  password{} jsonRet{}"
						  , message, password, jsonRet.dump());

			return jsonRet;
		}

		//key password ？
		std::string CDid::Sign(
			const std::string &message , const std::string &password){

			Log::getLogger()->info("CDid::Sign  begin message{}  password{}", message, password);

			ErrorChecker::argumentNotEmpty(password, "Password");
			CheckInit();
			nlohmann::json jsonRet = _didManger->_iidAgent->Sign( _didNameStr, message, password);//_didNameStr,

			Log::getLogger()->info("CDid::GenerateProgram  end message{}  password{} jsonRet{}"
				, message, password, jsonRet.dump());
			return jsonRet;
		}

		nlohmann::json CDid::CheckSign(
			const std::string &message ,
			const std::string &signature){

			Log::getLogger()->info("CDid::CheckSign  begin message{}  signature{}", message, signature);
			CheckInit();


			CMBlock signatureData(signature.size());
			memcpy(signatureData, signature.c_str(), signature.size());

			UInt256 md;
			BRSHA256(&md, message.c_str(), message.size());


			bool r = Key::verifyByPublicKey(GetPublicKey(), md, signatureData);
			nlohmann::json jsonData;
			jsonData["Result"] = r;

			Log::getLogger()->info("CDid::CheckSign  begin message{}  signature{} jsonData{}"
						  , message, signature, jsonData.dump());

			return jsonData;
		}

		std::string CDid::GetPublicKey() {
			Log::getLogger()->info("CDid::GetPublicKey  begin _didNameStr{}  ",  _didNameStr);

			CheckInit();
			std::string lostrPubkey = _didManger->_iidAgent->GetPublicKey(_didNameStr);
			// _didManger->_masterWallet->GetPublicKey();//_didNameStr  ??????

			Log::getLogger()->info("CDid::GetPublicKey  end _didNameStr{} lostrPubkey {}"
						  ,  _didNameStr, lostrPubkey);
			return lostrPubkey;
		}

	}
}