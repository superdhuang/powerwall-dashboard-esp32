#pragma once
// Runs in a temporary directory: never replaces the user's live history.
static int runWeeklyHistoryChecks() {
  auto check = [](bool passed, const char* message) {
    std::printf("[WeeklyHistoryTest] %s: %s\n", passed ? "PASS" : "FAIL", message);
    return passed;
  };
  tm day{}; day.tm_year=126; day.tm_mon=9; day.tm_mday=22; day.tm_hour=12; day.tm_isdst=-1;
  time_t now=mktime(&day);
  WeeklyHistory h; h.roll(now);
  if (!check(!strcmp(h.days[4].date,"2026-10-22") && !strcmp(h.days[5].date,"2026-10-16"), "recent seven dates in Sunday–Saturday order")) return 1;
  HistoryPoint p[]={{0,24,2,HistoryPowerSource::Grid},{6,80,4,HistoryPowerSource::Powerwall},{11,70,6,HistoryPowerSource::Unknown},{14,60,8,HistoryPowerSource::Powerwall}};
  h.merge("2026-10-22",p,4); h.merge("2026-10-22",p,4);
  auto avg=WeeklyHistory::averages(h.days[4],day);
  if (!check(h.days[4].points.size()==4 && avg[0]==5 && avg[1]==6 && avg[2]==4,"all-day / peak / off-peak averages; repeated polls do not duplicate")) return 1;
  tm weekend=WeeklyHistory::dayAt(now,4);
  h.merge("2026-10-18",p,4);
  avg=WeeklyHistory::averages(h.days[0],weekend);
  if (!check(std::isnan(avg[1]) && avg[2]==5,"weekends are entirely off-peak; missing peak stays empty")) return 1;
  if (!check(!WeeklyHistory::peak(day,5.99f) && WeeklyHistory::peak(day,6) && !WeeklyHistory::peak(day,11) && WeeklyHistory::peak(day,14),"non-summer boundaries")) return 1;
  tm summer=day; summer.tm_mon=5;
  if (!check(!WeeklyHistory::peak(summer,8.99f) && WeeklyHistory::peak(summer,9),"summer boundary")) return 1;
  HistoryPoint invalid[]={{NAN,24,2},{24,24,2},{1,101,2},{1,50,-1},{1,50,NAN}};
  h.merge("2026-10-22",invalid,5);
  if (!check(h.days[4].points.size()==4,"invalid observations rejected")) return 1;
  std::vector<HistoryPoint> full;
  for(int b=0;b<288;++b) full.push_back({b/12.f,50,2,HistoryPowerSource::Grid});
  h.merge("2026-10-21",full.data(),full.size());
  if (!check(h.days[3].points.size()==288,"five-minute storage preserves all 288 daily buckets")) return 1;
  char original[PATH_MAX]; if (!getcwd(original,sizeof(original))) return 1;
  char folder[]="/tmp/powerwall-week-test-XXXXXX";
  if (!mkdtemp(folder) || chdir(folder)) return 1;
  bool ok=saveWeeklyHistory(h);
  WeeklyHistory restored; restored.roll(now); readWeeklyFile(restored,"history_week.json",true);
  ok=check(ok && restored.days[4].points.size()==4 && restored.days[3].points.size()==288 && restored.days[4].points[1].power_source==HistoryPowerSource::Powerwall,"atomic JSON save and restart restore") && ok;
  JsonDocument legacy; deserializeJson(legacy,"{\"date\":\"2026-10-22\",\"points\":[{\"hour\":2,\"soe\":50,\"load_kw\":3}]}");
  importWeeklyJson(restored,legacy.as<JsonVariantConst>());
  ok=check(restored.days[4].points.size()==5 && restored.days[4].points[1].power_source==HistoryPowerSource::Grid,"existing today JSON imports without a source field") && ok;
  restored.roll(mktime(&day)+86400);
  ok=check(!strcmp(restored.days[5].date,"2026-10-23") && restored.days[5].points.empty() && restored.days[4].points.size()==5,"midnight retains yesterday and evicts the oldest day") && ok;
  tm january{}; january.tm_year=127; january.tm_mon=0; january.tm_mday=1; january.tm_hour=12; january.tm_isdst=-1;
  restored.roll(mktime(&january));
  ok=check(!strcmp(restored.days[4].date,"2026-12-31") && restored.days[4].points.empty(),"year rollover clears expired history") && ok;
  std::remove("history_week.json"); chdir(original); rmdir(folder);
  return ok?0:1;
}
