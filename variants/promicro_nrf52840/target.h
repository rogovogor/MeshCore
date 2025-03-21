#pragma once

#include <FaketecBoard.h>
#include <helpers/CustomSX1262Wrapper.h>
#include <helpers/CustomLLCC68Wrapper.h>

extern FaketecBoard board;
extern RADIO_CLASS radio;

bool radio_init();
