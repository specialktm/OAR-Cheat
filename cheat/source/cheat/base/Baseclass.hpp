#pragma once 
#include "pch/pch.h"

class Baseclass {
public:
	virtual void Initialize() = 0;
	virtual void Uninitialize() = 0;
	virtual std::string ClassName() = 0;
	virtual ~Baseclass() { }
};