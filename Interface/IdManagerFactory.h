// Copyright (c) 2012-2018 The Elastos Open Source Project
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef __ELASTOS_SDK_IDMANAGERFACTORY_H__
#define __ELASTOS_SDK_IDMANAGERFACTORY_H__

#include "IIdManager.h"

namespace Elastos {
	namespace SDK {

		class IdManagerFactory {
		public:
			IIdManager *CreateIdManager(const std::vector<std::string> &initialAddresses);
		};

	}
}

#endif //__ELASTOS_SDK_IDMANAGERFACTORY_H__
