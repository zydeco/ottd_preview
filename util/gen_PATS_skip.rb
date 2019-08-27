ottd_src = ARGV[0]
require 'pry'
# make settings.h: objs/setting/settings_gen src/table/settings.ini
settings_h = "#{ottd_src}/objs/setting/table/settings.h"

if !File.exist?settings_h then
  puts "file not found: #{settings_h}"
  exit
end

# read settings
settings_file = File.read(settings_h)
settings_struct = settings_file[/const SettingDesc _settings\[\] = \{\n(.+)\nSDT_VAR\(GameSettings, game_creation.starting_year/m,1]
# don't have to parse ifdefs, since there shouldn't be any before game_creation.starting_year

@skip = {}

SL_MAX_VERSION = 65535
SLV = {
  SL_MIN_VERSION: 0,
  SLV_EXTEND_CARGOTYPES: 199,
  SLV_EXTEND_RAILTYPES: 200,
  SLV_EXTEND_PERSISTENT_STORAGE: 201,
  SLV_EXTEND_INDUSTRY_CARGO_SLOTS: 202,
  SLV_SHIP_PATH_CACHE: 203,
  SLV_SHIP_ROTATION: 204,
  SLV_GROUP_LIVERIES: 205,
  SLV_SHIPS_STOP_IN_LOCKS: 206,
  SLV_FIX_CARGO_MONITOR: 207,
  SLV_TOWN_CARGOGEN: 208,
  SLV_SHIP_CURVE_PENALTY: 209,
  SLV_SERVE_NEUTRAL_INDUSTRIES: 210,
  SLV_ROADVEH_PATH_CACHE: 211,
  SLV_REMOVE_OPF: 212,
  SLV_TREES_WATER_CLASS: 213,
  SLV_ROAD_TYPES: 214,
  SLV_SCRIPT_MEMLIMIT: 215,
  SLV_MULTITILE_DOCKS: 216,
  SL_MAX_VERSION: SL_MAX_VERSION
}.freeze

def parse_slv slv
  if slv.match(/\ASLV_\d+\z/)
    return slv[4..-1].to_i
  elsif SLV.has_key?(slv.to_sym)
    return SLV[slv.to_sym]
  else
    puts "unknown saveload version %s, check src/saveload/saveload.h" % slv
    exit
  end
end

def add_skip type,from,to
  if type.is_a?Fixnum then 
    nbytes = type 
  elsif type.is_a?String then 
    nbytes = {'SLE_UINT8' => 1, 'SLE_UINT16' => 2, 'SLE_UINT32' => 4}[type]
    if nbytes == nil then
      puts "unknown type %s" % type
      exit
    end
  end
  
  from = parse_slv(from)
  to = parse_slv(to)
  
  if @skip.has_key?(from..to) then
    @skip[(from..to)] += nbytes
  else
    @skip[(from..to)] = nbytes
  end
end

settings_struct.lines.each do |line| 
  case line[/^(\w+)/,1]
  when 'SDT_NULL'
    match = line.match(/\((\d+), (\w+), (\w+)\)/)
    add_skip match[1].to_i,match[2],match[3]
  when 'SDT_VAR'
    match = line.match(/\(([^,]+),\s+([^,]+),\s+([^,]+).+,\s+([^,]+),\s+([^,]+),\s+([^,]+)\)/)
    add_skip match[3],match[4],match[5]
  when 'SDT_BOOL','SDTG_BOOL','SDTC_BOOL'
    match = line.match(/\s+([^,]+),\s+([^,]+),\s+([^,]+)\)/)
    add_skip 'SLE_UINT8', match[1], match[2]
  when 'SDTG_VAR'
    match = line.match(/\(([^,]+),\s+([^,]+),.+,\s+([^,]+),\s+([^,]+),\s+([^,]+)\)/)
    add_skip match[2],match[3],match[4]
  when 'SDT_OMANY'
    match = line.match(/\(([^,]+),\s+([^,]+),\s+([^,]+).+,\s+([^,]+),\s+([^,]+),\s+([^,]+),\s+([^,]+)\)/)
    add_skip match[3],match[4],match[5]
  else
    puts "I don't know what %s is" % line[/^(\w+)/,1]
    exit
  end
end

puts "// skip generated with gen_PATS_skip.rb"
puts "ottd_skip(fp, #{@skip[0..SL_MAX_VERSION]});"
@skip.delete(0..SL_MAX_VERSION)
@skip.each_pair { |range, nbytes|
  if range.min == 0 then
    puts "if (version <= #{range.max}) ottd_skip(fp, #{nbytes});"
  elsif range.max == SL_MAX_VERSION then
    puts "if (version >= #{range.min}) ottd_skip(fp, #{nbytes});"
  else
    puts "if (version >= #{range.min} && version <= #{range.max}) ottd_skip(fp, #{nbytes});"
  end
  
}
