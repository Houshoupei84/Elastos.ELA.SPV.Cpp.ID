// Copyright (c) 2012-2018 The Elastos Open Source Project
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "didManager.h"
#include "did.h"

#include "Core/BRTransaction.h"
#include "SDK/Common/Utils.h"
#include "SDK/Wrapper/Key.h"
#include "SDK/Wrapper/AddressRegisteringWallet.h"
#include "SDK/Implement/IdChainSubWallet.h"
#include "SDK/Implement/MasterWallet.h"
#include "SDK/Implement/SubWalletCallback.h"
#include "SDK/ELACoreExt/Payload/PayloadRegisterIdentification.h"
#include "SDK/ELACoreExt/ELATransaction.h"
#include "SDK/Common/ParamChecker.h"
#include "SDK/Common/Log.h"
#include "Interface/IMasterWallet.h"

#define SPV_DB_FILE_NAME "spv.db"
#define PEER_CONFIG_FILE "id_PeerConnection.json"
#define IDCACHE_DIR_NAME "IdCache"
#define IDCHAIN_NAME     "IdChain"


namespace fs = boost::filesystem;
using namespace Elastos::ElaWallet;



namespace Elastos {
	namespace DID {



		//ISubWalletCallback
		class SpvListener : public Wallet::Listener{
		public:
			SpvListener(CDidManager *manager) : _manager(manager) {
			}

			virtual void balanceChanged(uint64_t balance) {

			}

			virtual void onTxAdded(const TransactionPtr &transaction) {

				if (transaction->getTransactionType() != ELATransaction::RegisterIdentification)
					return;

				fireTransactionStatusChanged(
					static_cast<PayloadRegisterIdentification *>(transaction->getPayload().get()),
					SubWalletCallback::Added, transaction->getBlockHeight());
			}

			virtual void onTxUpdated(const std::string &hash, uint32_t blockHeight, uint32_t timeStamp) {
//				BRTransaction *transaction = BRWalletTransactionForHash(
//					_manager->_walletManager->getWallet()->getRaw(), Utils::UInt256FromString(hash));
//				if (transaction == nullptr ||
//					((ELATransaction *) transaction)->type != ELATransaction::RegisterIdentification)
//					return;
//
//				Transaction wrapperTx((ELATransaction *)transaction);
//				PayloadRegisterIdentification *payload = static_cast<PayloadRegisterIdentification *>(
//					wrapperTx.getPayload().get());
//				fireTransactionStatusChanged(payload, SubWalletCallback::Updated, blockHeight);
			}

			virtual void onTxDeleted(const std::string &hash, bool notifyUser, bool recommendRescan) {
//				BRTransaction *transaction = BRWalletTransactionForHash(
//					_manager->_walletManager->getWallet()->getRaw(), Utils::UInt256FromString(hash));
//				if (transaction == nullptr ||
//					((ELATransaction *) transaction)->type != ELATransaction::RegisterIdentification)
//					return;
//
//				Transaction wrapperTx((ELATransaction *)transaction);
//				PayloadRegisterIdentification *payload = static_cast<PayloadRegisterIdentification *>(
//					wrapperTx.getPayload().get());
//				fireTransactionStatusChanged(payload, SubWalletCallback::Deleted, transaction->blockHeight);
			}



		private:
			void fireTransactionStatusChanged(PayloadRegisterIdentification *payload,
											  SubWalletCallback::TransactionStatus status, uint32_t blockHeight) {
				nlohmann::json j = payload->toJson();
				if (j.find("ID") == j.end())
					return;

				_manager->OnTransactionStatusChanged(j["ID"].get<std::string>(), SubWalletCallback::convertToString(
					status), j, blockHeight);
			}

			CDidManager *_manager;
		};

		CDidManager::~CDidManager() {

			DidMap::iterator itor ;
			CDid* did = NULL;

			for (itor = _didMap.begin(); itor !=  _didMap.end(); ) {

				did = (CDid*)itor->second;
				delete did;
				_didMap.erase(itor++);

			}

		}

		CDidManager::CDidManager(IMasterWallet* masterWallet, const std::string &rootPath)
			: _pathRoot(rootPath) {

			Log::getLogger()->set_level(spdlog::level::err);

			Log::getLogger()->info("CDidManager::CDidManager rootPath = {} begin", rootPath);

			ParamChecker::checkNullPointer(masterWallet);

			_masterWallet = (Elastos::ElaWallet::MasterWallet*)masterWallet;
			//_iidAgent     = (Elastos::ElaWallet::IIdAgent*)masterWallet;
			_iidAgent     = dynamic_cast<Elastos::ElaWallet::IIdAgent*>(_masterWallet);
			ParamChecker::checkNullPointer(_iidAgent);
			initSpvModule();
			initIdCache();
			Log::getLogger()->info("CDidManager::CDidManager rootPath = {} end", rootPath.c_str());
		}

		IDID * CDidManager::CreateDID(const std::string &password){

			Log::getLogger()->info("CDidManager::CreateDID password = {} begin", password);

			ParamChecker::checkPassword(password);

			std::vector<std::string> allIdsVec = _masterWallet->GetAllIds();

			uint32_t index =  allIdsVec.size();

			std::string didNameStr = "";
			didNameStr = _iidAgent->DeriveIdAndKeyForPurpose(1 , index , password);

			std::cout<<"new didNameStr " <<didNameStr <<std::endl;
			nlohmann::json defJson ={
				{"121", {{"datahash", "datahash1"}, {"proof", "hello proof1"}, {"sign", "hello sign1"}}},
				{"132", {{"datahash", "datahash2"}, {"proof", "hello proof2"}, {"sign", "hello sign2"}}},
				{"103", {{"datahash", "datahash3"}, {"proof", "hello proof3"}, {"sign", "hello sign3"}}}
			};
			//notice didNameStr must be store in leveldb before user setValue
			//so path is ""  is not user data
			_idCache->Put(didNameStr, "", defJson);
			return NewDid(didNameStr);
		}

		IDID * CDidManager::GetDID(const std::string &didName)  {

			if (_didMap.find(didName) == _didMap.end())
				return NULL;

			return  _didMap[didName];
		}

		nlohmann::json CDidManager::GetDIDList() const {

//			nlohmann::json didJson;
//
//			DidMap::const_iterator itor  = _didMap.begin() ;
//			for (itor  =  _didMap.begin() ; itor != _didMap.end(); itor++) {
//
//				didJson.push_back(itor->first);
//			}
//			return didJson ;
			Log::getLogger()->info("GetDIDList begin");

			return _idCache->GetAllKey();
		}

		void  CDidManager::DestoryDID(const std::string &didName){

			Log::getLogger()->info("DestoryDID didName {} begin",  didName);
			DidMap::iterator itor ;
			itor = _didMap.find(didName);
			if (itor == _didMap.end())
				return;

			CDid * idID = (CDid *)itor->second;
			_didMap.erase(itor);
			delete idID;

			_idCache->Delete(didName);
			Log::getLogger()->info("DestoryDID didName {} end",  didName);

		}



		nlohmann::json CDidManager::GetLastIdValue(const std::string &id, const std::string &path)  {
			ParamChecker::checkNotEmpty(id);
			ParamChecker::checkNotEmpty(path);

			CDid * idID  = (CDid *)GetDID(id);
			return idID->GetValue(path);

		}

		void
		CDidManager::OnTransactionStatusChanged(const std::string &id, const std::string &status,
											  const nlohmann::json &desc, uint32_t blockHeight) {

			Log::getLogger()->info("CDidManager::OnTransactionStatusChanged");
			std::string path = desc["Path"].get<std::string>();
			nlohmann::json value;
			if (status == "Added" || status == "Updated") {
				value.push_back(desc["DataHash"]);
				value.push_back(desc["Proof"]);
				value.push_back(desc["Sign"]);
				Log::getLogger()->info("CDidManager::OnTransactionStatusChanged before updateDatabase");
				updateDatabase(id, path, value, blockHeight);
			} else if (status == "Deleted") {
				value = GetLastIdValue(id, path);

				Log::getLogger()->info("CDidManager::OnTransactionStatusChanged before removeIdItem");

				removeIdItem(id, path, blockHeight);
			}

			if (_idListenerMap.find(id) != _idListenerMap.end())
				_idListenerMap[id]->FireCallbacks(id, status, value);
		}

		void CDidManager::initSpvModule() {

//			fs::path dbPath = _pathRoot;
//			dbPath /= SPV_DB_FILE_NAME;
//			fs::path peerConfigPath = _pathRoot;
//			peerConfigPath /= PEER_CONFIG_FILE;
//			////
//			static nlohmann::json ElaPeerConfig =
//				R"(
//						  {
//							"MagicNumber": 7630401,
//							"KnowingPeers":
//							[
//								{
//									"Address": "127.0.0.1",
//									"Port": 20866,
//									"Timestamp": 0,
//									"Services": 1,
//									"Flags": 0
//								}
//							]
//						}
//					)"_json;
			///
			//WalletManager future use
//			std::vector<std::string>  initialAddresses;
//			_walletManager = WalletManagerPtr(
//				new WalletManager(dbPath, ElaPeerConfig, 0, 0, initialAddresses, ChainParams::mainNet()));
//
			//_walletManager->registerWalletListener(_spvListener.get());
			//_masterWallet->
		}

		void CDidManager::updateDatabase(const std::string &id, const std::string &path, const nlohmann::json &value,
									   uint32_t blockHeight) {
			//todo consider block height equal INT_MAX(unconfirmed)
			//todo parse proof, data hash, sign from value


			ParamChecker::checkNotEmpty(id);
			ParamChecker::checkNotEmpty(path);

			CDid * idID  = (CDid *)GetDID(id);
			return idID->setValue(path, value, blockHeight);
		}

		void CDidManager::removeIdItem(const std::string &id, const std::string &path, uint32_t blockHeight) {
			ParamChecker::checkNotEmpty(id);
			ParamChecker::checkNotEmpty(path);

			CDid * idID  = (CDid *)GetDID(id);
			idID->DelValue(path, blockHeight);

		}

		ISubWallet * CDidManager::GetIDSubWallet(){
			Log::getLogger()->info("GetIDSubWallet  begin");

			std::vector<ISubWallet *> subWalletVec;
			subWalletVec = _masterWallet->GetAllSubWallets();
			ISubWallet * subWallet = NULL;

			for (int i = 0; i < subWalletVec.size(); ++i) {
				subWallet = subWalletVec[i];
				if (subWallet->GetChainId() ==  IDCHAIN_NAME){
					break;
				}
				subWallet = NULL;
			}
			Log::getLogger()->info("GetIDSubWallet subWallet int: {:p} end", ( void* )subWallet);//{0:x}
			return subWallet;
		}

		bool CDidManager::initIdCache() {

			Log::getLogger()->info("initIdCache  begin");
			if (_idCache != NULL){
				Log::getLogger()->info("initIdCache  _idCache != NULL end");
				return false;
			}


			fs::path idCachePath = _pathRoot;
			idCachePath /= IDCACHE_DIR_NAME;

			_idCache =IdCachePtr(new IdCache(idCachePath));


			ISubWallet * subWallet = GetIDSubWallet();
			if (subWallet){
				//新加
				ISubWalletCallback *subCallback = dynamic_cast<ISubWalletCallback *>(this);
				//注册回调
				subWallet->AddCallback(subCallback);

				nlohmann::json transJson;
				nlohmann::json allTransJsonRet = subWallet->GetAllTransaction(0
					, 10000 , "");
				transJson = allTransJsonRet["Transactions"];

				Transaction transaction;
				for (nlohmann::json::const_iterator it = transJson.begin(); it != transJson.end(); it++) {
					//transAction = it.value();
					transaction.fromJson(it.value());


					PayloadRegisterIdentification *payload =
						static_cast<PayloadRegisterIdentification *>(transaction.getPayload().get());
					uint32_t blockHeight = transaction.getBlockHeight();

					nlohmann::json jsonToSave = payload->toJson();
					jsonToSave.erase(payload->getId());
					jsonToSave.erase(payload->getPath());
					//if id  is mine
					//NewDid(payload->getId());
					_idCache->Put(payload->getId(), payload->getPath(), blockHeight, jsonToSave);
				}
			}

			nlohmann::json jsonRet = _idCache->GetAllKey();

			//std::cout << "jsonRet "<< jsonRet <<std::endl;

			for (nlohmann::json::const_iterator it = jsonRet.begin(); it != jsonRet.end(); it++) {
				//std::cout << "value "<< it.value()<<std::endl;
				NewDid(it.value());
			}
			Log::getLogger()->info("initIdCache  end");
			return true;
		}


		bool	CDidManager::RegisterCallback(const std::string &id, IIdManagerCallback *callback) {
			Log::getLogger()->info("RegisterCallback  begin id {} callback {:p}", id, ( void* )callback);

			if (_idListenerMap.find(id) == _idListenerMap.end()) {
				_idListenerMap[id] = ListenerPtr(new SubWalletListener(this));
			}

			_idListenerMap[id]->AddCallback(callback);
			Log::getLogger()->info("RegisterCallback  end id {} callback {:p}", id, ( void* )callback);

			return true;
		}

		bool CDidManager::UnregisterCallback(const std::string &id) {
			Log::getLogger()->info("UnregisterCallback  begin id {} ", id);

			if (_idListenerMap.find(id) == _idListenerMap.end())
				return false;

			_idListenerMap.erase(id);
			Log::getLogger()->info("UnregisterCallback  end id {} ", id);

			return true;
		}

		IDID *  CDidManager::NewDid(const std::string didNameStr) {
			Log::getLogger()->info("NewDid  begin didNameStr {} ", didNameStr);

			CDid * idID = new  CDid(this, didNameStr);
			_didMap[didNameStr] =  idID;
			RegisterId(didNameStr);
			Log::getLogger()->info("NewDid  end didNameStr {} ", didNameStr);

			return idID;
		}

		void CDidManager::RegisterId(const std::string &id) {

			//WalletManager future use
			//AddressRegisteringWallet *wallet = static_cast<AddressRegisteringWallet *>(_walletManager->getWallet().get());
			//wallet->RegisterAddress(id);


		}

		void CDidManager::RecoverIds(const std::vector<std::string> &ids, const std::vector<std::string> &keys,
								   const std::vector<std::string> &passwords) {
			//WalletManager future use
//			for (int i = 0; i < ids.size(); ++i) {
//				RegisterId(ids[i]);
//			}
//			_walletManager->getPeerManager()->rescan();
		}


		CDidManager::SubWalletListener::SubWalletListener(CDidManager *manager) : _manager(manager) {

		}
		void CDidManager::SubWalletListener::FireCallbacks(const std::string &id, const std::string &path,
														 const nlohmann::json &value) {
			Log::getLogger()->info("FireCallbacks  begin didNameStr id {} path{} value{}", id , path, value.dump());

			std::for_each(_callbacks.begin(), _callbacks.end(), [&id, &path, &value](IIdManagerCallback *callback) {
				callback->OnIdStatusChanged(id, path, value);
			});
			Log::getLogger()->info("FireCallbacks  end didNameStr id {} path{} value{}", id , path, value.dump());
		}

		void CDidManager::SubWalletListener::AddCallback(IIdManagerCallback *managerCallback) {
			Log::getLogger()->info("AddCallback  begin didNameStr {:p} ", ( void* )managerCallback);

			if (std::find(_callbacks.begin(), _callbacks.end(), managerCallback) != _callbacks.end())
				return;
			_callbacks.push_back(managerCallback);
			Log::getLogger()->info("AddCallback  end didNameStr {:p} ", ( void* )managerCallback);

		}

		void CDidManager::SubWalletListener::RemoveCallback(IIdManagerCallback *managerCallback) {
			Log::getLogger()->info("RemoveCallback  begin didNameStr {:p} ", ( void* )managerCallback);

			_callbacks.erase(std::remove(_callbacks.begin(), _callbacks.end(), managerCallback), _callbacks.end());

			Log::getLogger()->info("RemoveCallback  end didNameStr {:p} ", ( void* )managerCallback);

		}
	}

}