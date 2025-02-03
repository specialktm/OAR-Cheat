#pragma once
#include "pch/pch.h"
namespace auth
{
	class auth
	{
	public:
		auth(std::string_view key);
		bool IsAuthed()
		{
			return authed;
		}
		bool Updated();
	private:
		bool authed = false;
	};
}