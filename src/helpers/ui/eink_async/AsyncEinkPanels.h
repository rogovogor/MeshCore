#pragma once

// Registry mapping a GxEPD2 panel type to its non-blocking adapter.
//
// The build configures the physical panel exactly once, via EINK_DISPLAY_MODEL,
// and turns the async path on with WITH_ASYNC_EINK. The adapter follows from
// the model, so a config cannot name a panel and an adapter that disagree —
// there used to be a second EINK_ASYNC_PANEL=213/290 flag carrying the same
// fact, and nothing checked the two against each other.
//
// Adding a panel: write its adapter and add one specialisation below. An
// unsupported model fails the build on the missing Type, naming the model.

#include "AsyncGxEPD213B74.h"
#include "AsyncGxEPD290T94V2.h"
#include "AsyncGxEPD213BN.h"
#include "AsyncHeltec213.h"
#include "AsyncGxEPD290BS.h"

template <typename Panel>
struct AsyncEinkPanelFor {
  static_assert(sizeof(Panel) == 0,
                "No async e-ink adapter for this EINK_DISPLAY_MODEL. Add a "
                "specialisation in AsyncEinkPanels.h, or build without "
                "WITH_ASYNC_EINK to use the stock synchronous GxEPD2 driver.");
};

template <>
struct AsyncEinkPanelFor<GxEPD2_213_B74> {
  using Type = AsyncGxEPD213B74;
};

template <>
struct AsyncEinkPanelFor<GxEPD2_290_T94_V2> {
  using Type = AsyncGxEPD290T94V2;
};

// Реальные панели Heltec Vision Master: DEPG0290BNS800F6 (E290) и LCMEN2R13EFC1
// (E213). Адаптеры — потомки классов GxEPD2 того же семейства.
template <>
struct AsyncEinkPanelFor<GxEPD2_290_BS> { using Type = AsyncGxEPD290BS; };

template <>
struct AsyncEinkPanelFor<GxEPD2_213_BN> { using Type = AsyncGxEPD213BN; };

// 2.13" панели Heltec (Vision Master E213, Wireless Paper): контроллер
// определяется при старте, поэтому модель и адаптер — один класс.
template <>
struct AsyncEinkPanelFor<AsyncHeltec213> {
  using Type = AsyncHeltec213;
};
