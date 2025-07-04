#!/usr/bin/awk -f
#
BEGIN {timeline = 0; xtimeline = 0;}
/\\[^nrt]/ {gsub(/\\n/,"@n");gsub(/\\r/,"@r");gsub(/\\/,"/");gsub(/@/,"\\")}
/<time>[ \t]+=>[ \t]+\|[0-9]{12}\|/ {sub(/=>[ \t]+\|[0-9]{12}\|/,"=> |:time:|")}
/<xtime>[ \t]+=>[ \t]+\|[0-9]+\|/ {sub(/=>[ \t]+\|[0-9]+\|/,"=> |:xtime:|")}
/[;]time=[0-9]{12}/ {sub(/[;]time=[0-9]{12}/,";time=:time:")}
/[;]xtime=[0-9]+/ {sub(/[;]xtime=[0-9]+/,";xtime=:xtime:")}
/^--- Testing: time$/ {timeline = 1}
timeline == 1 && /^[0-9]{12}$/ {timeline = 0; sub(/^[0-9]{12}$/,":time:")}
/^--- Testing: xtime$/ {xtimeline = 1}
xtimeline == 1 && /^[0-9]+$/ {xtimeline = 0; sub(/^[0-9]+$/,":xtime:")}
/<argv>[ \t]+=>[ \t]+\|.*ttm(\.exe)?\|/ {sub(/\|.*ttm(\.exe)?\|/,"|ttm.exe|")}
/<wd>[ \t]+=>[ \t]+\|[a-zA-z]:*ttm(\.exe)?\|/ {sub(/\|.*ttm(\.exe)?\|/,"|ttm.exe|")}
/\|.*\/ttm\/src/ {sub(/\|.*\/ttm\/src/,"|/ttm/src")}
/;WD;.*\/ttm\/src/ {sub(/;WD;.*\/ttm\/src/,";WD;/ttm/src")}
/<include;.*\/ttm\/src/ {sub(/<include;.*\/ttm\/src/,"<include;/ttm/src")}
/^([a-zA-Z]:)?(\/[^\/]+)*\/ttm\/src/ {sub(/^([a-zA-Z]:)?(\/[^\/]+)*\/ttm\/src/,"/ttm/src")}
/.*/
