ottd_src = File.expand_path('~/Documents/OpenTTD/src/trunk/')

# make settings.h: objs/setting/settings_gen src/table/settings.ini
settings_h = "#{ottd_src}/objs/setting/table/settings.h"

if !File.exist?settings_h then
  puts "settings.h doesn't exist, but I want it"
  exit
end

# read settings
settings_file = File.read(settings_h)
settings_struct = settings_file[/const SettingDesc _settings\[\] = \{\n(.+)\nSDT_VAR\(GameSettings, game_creation.starting_year/m,1]
# don't have to parse ifdefs, since there shouldn't be any before game_creation.starting_year

@skip = {}

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
  
  from = (from == 'SL_MAX_VERSION') ? 255 : from.to_i
  to = (to == 'SL_MAX_VERSION') ? 255 : to.to_i
  
  if @skip.has_key?(from..to) then
    @skip[(from..to)] += nbytes
  else
    @skip[(from..to)] = nbytes
  end
end

settings_struct.lines.each { |line| 
  case line[/^(\w+)/,1]
  when 'SDT_NULL'
    match = line.match(/\((\d+), (\d+), (\d+)\)/)
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
}

puts "// skip generated with gen_PATS_skip.rb"
puts "ottd_skip(fp, #{@skip[0..255]});"
@skip.delete(0..255)
@skip.each_pair { |range, nbytes|
  if range.min == 0 then
    puts "if (version <= #{range.max}) ottd_skip(fp, #{nbytes});"
  elsif range.max == 255 then
    puts "if (version >= #{range.min}) ottd_skip(fp, #{nbytes});"
  else
    puts "if (version >= #{range.min} && version <= #{range.max}) ottd_skip(fp, #{nbytes});"
  end
  
}
