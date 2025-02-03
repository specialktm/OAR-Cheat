#include "auth.h"
#include "auth/auth.hpp"
#include "auth/skStr.h"
namespace auth
{
	static KeyAuth::api::appdata appData;

	auth::auth(std::string_view key)
	{
		std::string name = skCrypt("Sub").decrypt();
		std::string ownerid = skCrypt("4GPs2oVcgC").decrypt();
		std::string secret = skCrypt("aca1a0920a71701104c2edb4ea2c9438120ba4c90106b5ec95ebabf2668b74ec").decrypt();
		std::string version = skCrypt("1.1").decrypt(); 
		std::string url = skCrypt("https://keyauth.win/api/1.3/").decrypt();
		std::string path = skCrypt("").decrypt(); 
		KeyAuth::api KeyAuthApp(name, ownerid, version, url, path);
		KeyAuthApp.init();
		KeyAuthApp.license(key.data(), "");

		if (KeyAuthApp.response.message.empty()) authed = false;

		if (KeyAuthApp.response.success)
		{
			authed = true;
		}

		appData = KeyAuthApp.app_data;
	}

	bool auth::Updated()
	{
		if (appData.version == APP_VERSION)
		{
			return false;
		}
		return true;
	}
}