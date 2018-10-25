// Copyright (c) 2012-2018 The Elastos Open Source Project
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef IDCHAIN_DIDMANAGER_H
#define IDCHAIN_DIDMANAGER_H

#include <map>
#include <Interface/IIdManagerCallback.h>

#include "nlohmann/json.hpp"
#include "Interface/ididManager.h"
//#include "SDK/SpvService/WalletManager.h"
#include "IdCache.h"
//#include <SDK/IdAgent/IdAgentImpl.h>
//#include "MasterWallet.h"
#include "Interface/IIdAgent.h"
#include "Interface/ISubWallet.h"
#include "Interface/ISubWalletCallback.h"
#include "Interface/IMasterWallet.h"
#include "Plugin/Interface/IMerkleBlock.h"
namespace Elastos {
	namespace DID {
		class SpvListener;


		class CDidManager : public IDIDManager/*, public ElaWallet::ISubWalletCallback*/
		{
		public:

			CDidManager(ElaWallet::IMasterWallet* masterWallet, const std::string &rootPath) ;

			virtual ~CDidManager();

			virtual IDID * CreateDID(const std::string &password);

			virtual IDID * GetDID(const std::string &didName) ;
			//all id
			virtual nlohmann::json GetDIDList() const;

			virtual void  DestoryDID(const std::string &didName);

			virtual void ClearLocal();

			virtual bool  RegisterCallback(const std::string &id, IIdManagerCallback *callback);
			virtual bool  UnregisterCallback(const std::string &id);

//			virtual void OnTransactionStatusChanged(
//				const std::string &txID,
//				const std::string &status,
//				const nlohmann::json &desc,
//				uint32_t blockHeight);

		protected:
			friend class SpvListener;

/*
 * virtual void OnTransactionStatusChanged(
					const std::string &txid,
					const std::string &status,
					const nlohmann::json &desc,
					uint32_t confirms) = 0;
 * */


			void updateDatabase(const std::string &id,
								const std::string &path,
								const nlohmann::json &value,
								uint32_t blockHeight);

			virtual nlohmann::json GetLastIdValue(
				const std::string &id,
				const std::string &path) ;

			void removeIdItem(const std::string &id,
							  const std::string &path,
							  uint32_t blockHeight);

			void initSpvModule();

			bool initIdCache();


			void  RecoverIds(const std::vector<std::string> &ids, const std::vector<std::string> &keys,
									   const std::vector<std::string> &passwords);

			void  RegisterId(const std::string &id);

			IDID*  NewDid(const std::string didNameStr);

			ElaWallet::ISubWallet * GetIDSubWallet();

		protected:
			class SubWalletListener {
			public:
				SubWalletListener(CDidManager *manager);

				void AddCallback(IIdManagerCallback *managerCallback);

				void RemoveCallback(IIdManagerCallback *managerCallback);

				void FireCallbacks(const std::string &id,
								   const std::string &path,
								   const nlohmann::json &value);

			private:
				CDidManager *_manager;
				std::vector<IIdManagerCallback *> _callbacks;
			};

		protected:
			typedef std::map<std::string, IDID *> DidMap;


			typedef std::map<std::string, CMBlock> IdKeyMap;

			typedef boost::shared_ptr<SubWalletListener> ListenerPtr;
			typedef std::map<std::string, ListenerPtr> IdListenerMap;

			//typedef boost::shared_ptr<ElaWallet::WalletManager> WalletManagerPtr;

			typedef boost::shared_ptr<IdCache> IdCachePtr;
			friend class CDid;
			DidMap	_didMap;


			IdListenerMap _idListenerMap;


			//WalletManagerPtr _walletManager;

			std::string _pathRoot;
			IdCachePtr _idCache;


			Elastos::ElaWallet::IMasterWallet*	_masterWallet;
			//IIdAgent
			Elastos::ElaWallet::IIdAgent*	    _iidAgent;
		};
	}
}

#endif //IDCHAIN_DIDMANAGER_H
