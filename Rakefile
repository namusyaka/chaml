require "bundler/gem_tasks"
require "rake/extensiontask"
require 'rake/testtask'

Rake::ExtensionTask.new do |ext|
  ext.name    = 'engine'
  ext.config_script = "extconf.rb"
  ext.ext_dir = 'ext/chaml/engine'
  ext.lib_dir = 'lib/chaml'
  ext.tmp_dir = 'tmp/'
end

Rake::TestTask.new(:test) do |test|
  test.libs << 'test'
  test.test_files = Dir['test/**/test_*.rb']
  test.verbose = true
end

task :default => :install
task :spec => :install
