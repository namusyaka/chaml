require 'mkmf'

# NOTE
# requirements:
#  compiler:
#    clang ~> 2.9
#   or
#    gcc ~> 4.4
#   or
#    compiler that supporting C++11 extension N1737, N1984, N2242
requirements = {clang: '2.9', gcc: '4.4'}

#compiler = :clang
compiler=:gcc
config = {}
case compiler
when :clang
  config = {
    'CC' => 'clang',
    'CXX' => 'clang++',
    'cflags' => ([
      '-MMD',
      '-MP',
    ].join ' '),
    'CXXFLAGS' => ([
      '--std=c++11',
      '-Weverything',
      '-Wno-unknown-warning-option',
      '-Wno-c99-extensions',
      '-Wno-c++98-compat-pedantic',
      '-Wno-documentation-unknown-command',
      '-Wno-unreachable-code',
      '-Wno-missing-prototypes',
      '-Wno-deprecated-register',
      '-Wno-padded',
      '-Wno-gnu-designator',
      '-U__GNUC__',
      '-D__CLANG__',
      '-MMD',
      '-MP',
    ].join ' '),
  }
when :gcc
  config = {
    'CC' => 'gcc',
    'CXX' => 'g++',
    'cflags' => ([
      '-MMD',
      '-MP',
    ].join ' '),
    'CXXFLAGS' => ([
      '-Wall',
      '-Wempty-body',
      '-Wsign-compare',
      '-Wuninitialized',
      '-Wunused-parameter',
      '--std=c++0x',
      '-MMD',
      '-MP',
    ].join ' '),
  }
end

if requirements[compiler]
  version_regexp = /([0-9]+((\.[0-9]+)+))/
  `#{config['CC']} --version`.gsub version_regexp do |s|
    s =~ version_regexp
    #raise "requirements: #{compiler} ~> #{requirements[compiler]}" if $1 < requirements[compiler]
  end
end

RbConfig::MAKEFILE_CONFIG.merge! config

create_makefile('chaml/engine')
