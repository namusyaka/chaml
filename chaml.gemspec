# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'chaml/version'

Gem::Specification.new do |spec|
  spec.name          = "chaml"
  spec.version       = CHaml::VERSION
  spec.authors       = ["pixie", "namusyaka"]
  spec.email         = ["himajinn13sei@gmail.com", "namusyaka@gmail.com"]
  spec.description   = 'A Haml implementation written in C/C++'
  spec.summary       = spec.description
  spec.homepage      = "https://github.com/Luilak/chaml"
  spec.license       = "MIT"

  spec.files         = `git ls-files`.split($/)
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ["lib"]

  spec.extensions    = 'ext/chaml/engine/extconf.rb'

  spec.add_development_dependency "bundler", "~> 1.3"
  spec.add_development_dependency "rake"
end
