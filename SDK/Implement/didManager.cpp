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
#include <SDK/Common/ParamChecker.h>
#include "Interface/IMasterWallet.h"

#define SPV_DB_FILE_NAME "spv.db"
#define PEER_CONFIG_FILE "id_PeerConnection.json"
#define IDCACHE_DIR_NAME "IdCache"

namespace fs = boost::filesystem;
using namespace Elastos::SDK;

namespace Elastos {
	namespace DID {

		class SpvListener : public Wallet::Listener {
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
				BRTransaction *transaction = BRWalletTransactionForHash(
					_manager->_walletManager->getWallet()->getRaw(), Utils::UInt256FromString(hash));
				if (transaction == nullptr ||
					((ELATransaction *) transaction)->type != ELATransaction::RegisterIdentification)
					return;

				Transaction wrapperTx((ELATransaction *)transaction);
				PayloadRegisterIdentification *payload = static_cast<PayloadRegisterIdentification *>(
					wrapperTx.getPayload().get());
				fireTransactionStatusChanged(payload, SubWalletCallback::Updated, blockHeight);
			}

			virtual void onTxDeleted(const std::string &hash, bool notifyUser, bool recommendRescan) {
				BRTransaction *transaction = BRWalletTransactionForHash(
					_manager->_walletManager->getWallet()->getRaw(), Utils::UInt256FromString(hash));
				if (transaction == nullptr ||
					((ELATransaction *) transaction)->type != ELATransaction::RegisterIdentification)
					return;

				Transaction wrapperTx((ELATransaction *)transaction);
				PayloadRegisterIdentification *payload = static_cast<PayloadRegisterIdentification *>(
					wrapperTx.getPayload().get());
				fireTransactionStatusChanged(payload, SubWalletCallback::Deleted, transaction->blockHeight);
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

		}

		CDidManager::CDidManager(IMasterWallet* masterWallet , const std::vector<std::string> &initialAddresses)
			: _pathRoot("Data") {

			_masterWallet = (Elastos::SDK::MasterWallet*)masterWallet;
			initSpvModule(initialAddresses);
			initIdCache();
		}

		IDID * CDidManager::CreateDID(const std::string &password){

			CDid * idID = new  CDid(this);

			std::string didNameStr = "";
			didNameStr = idID->GetDIDName(password);
			_didMap[didNameStr] =  idID;

			RegisterId(didNameStr);
			return idID;
		}

		IDID * CDidManager::GetDID(const std::string &didName)  {

			if (_didMap.find(didName) == _didMap.end())
				return NULL;

			return  _didMap[didName];
		}

		nlohmann::json CDidManager::GetDIDList() const {

			return nlohmann::json() ;
		}

		void  CDidManager::DestoryDID(const std::string &didName){

			DidMap::iterator itor ;
			itor = _didMap.find(didName);
			if (itor == _didMap.end())
				return;

			CDid * idID = (CDid *)itor->second;
			_didMap.erase(itor);
			delete idID;
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

			std::string path = desc["Path"].get<std::string>();
			nlohmann::json value;
			if (status == "Added" || status == "Updated") {
				value.push_back(desc["DataHash"]);
				value.push_back(desc["Proof"]);
				value.push_back(desc["Sign"]);

				updateDatabase(id, path, value, blockHeight);
			} else if (status == "Deleted") {
				value = GetLastIdValue(id, path);

				removeIdItem(id, path, blockHeight);
			}

			if (_idListenerMap.find(id) != _idListenerMap.end())
				_idListenerMap[id]->FireCallbacks(id, status, value);
		}

		void CDidManager::initSpvModule(const std::vector<std::string> &initialAddresses) {

			fs::path dbPath = _pathRoot;
			dbPath /= SPV_DB_FILE_NAME;
			fs::path peerConfigPath = _pathRoot;
			peerConfigPath /= PEER_CONFIG_FILE;
			////
			static nlohmann::json ElaPeerConfig =
				R"(
						  {
							"MagicNumber": 7630401,
							"KnowingPeers":
							[
								{
									"Address": "127.0.0.1",
									"Port": 20866,
									"Timestamp": 0,
									"Services": 1,
									"Flags": 0
								}
							]
						}
					)"_json;
			///
			_walletManager = WalletManagerPtr(
				new WalletManager(dbPath, ElaPeerConfig, 0, 0, initialAddresses, ChainParams::mainNet()));

			_walletManager->registerWalletListener(_spvListener.get());

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

		bool CDidManager::initIdCache() {
			if (_idCache.Initialized())
				return true;

			fs::path idCachePath = _pathRoot;
			idCachePath /= IDCACHE_DIR_NAME;

			_idCache = IdCache(idCachePath);

			SharedWrapperList<Transaction, BRTransaction *> transactions =
				_walletManager->getTransactions(
					[](const TransactionPtr &transaction) {
						return transaction->getTransactionType() == ELATransaction::RegisterIdentification;
					});
			std::for_each(transactions.begin(), transactions.end(),
						  [this](const TransactionPtr &transaction) {
							  PayloadRegisterIdentification *payload =
								  static_cast<PayloadRegisterIdentification *>(transaction->getPayload().get());
							  uint32_t blockHeight = transaction->getBlockHeight();

							  nlohmann::json jsonToSave = payload->toJson();
							  jsonToSave.erase(payload->getId());
							  jsonToSave.erase(payload->getPath());
							  _idCache.Put(payload->getId(), payload->getPath(), blockHeight, jsonToSave);
						  });
			return true;
		}


		bool	CDidManager::RegisterCallback(const std::string &id, IIdManagerCallback *callback) {
			if (_idListenerMap.find(id) == _idListenerMap.end()) {
				_idListenerMap[id] = ListenerPtr(new SubWalletListener(this));
			}

			_idListenerMap[id]->AddCallback(callback);
			return true;
		}

		bool CDidManager::UnregisterCallback(const std::string &id) {
			if (_idListenerMap.find(id) == _idListenerMap.end())
				return false;

			_idListenerMap.erase(id);
			return true;
		}



		void CDidManager::RegisterId(const std::string &id) {

			AddressRegisteringWallet *wallet = static_cast<AddressRegisteringWallet *>(_walletManager->getWallet().get());
			wallet->RegisterAddress(id);


		}

		void CDidManager::RecoverIds(const std::vector<std::string> &ids, const std::vector<std::string> &keys,
								   const std::vector<std::string> &passwords) {
			for (int i = 0; i < ids.size(); ++i) {
				RegisterId(ids[i]);
			}
			_walletManager->getPeerManager()->rescan();
		}


		CDidManager::SubWalletListener::SubWalletListener(CDidManager *manager) : _manager(manager) {

		}
		void CDidManager::SubWalletListener::FireCallbacks(const std::string &id, const std::string &path,
														 const nlohmann::json &value) {
			std::for_each(_callbacks.begin(), _callbacks.end(), [&id, &path, &value](IIdManagerCallback *callback) {
				callback->OnIdStatusChanged(id, path, value);
			});
		}

		void CDidManager::SubWalletListener::AddCallback(IIdManagerCallback *managerCallback) {
			if (std::find(_callbacks.begin(), _callbacks.end(), managerCallback) != _callbacks.end())
				return;
			_callbacks.push_back(managerCallback);
		}

		void CDidManager::SubWalletListener::RemoveCallback(IIdManagerCallback *managerCallback) {
			_callbacks.erase(std::remove(_callbacks.begin(), _callbacks.end(), managerCallback), _callbacks.end());
		}
	}

}