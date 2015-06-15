require 'helper'
require 'json'

describe "Haml Spec" do
  contexts = JSON.parse(File.read(File.dirname(__FILE__) + "/fixtures/tests.json"))
  contexts.each do |context|
    context[1].each do |name, test|
      define_method("test_spec: #{name} (#{context[0]})") do
        scope            = Object.new
        html             = test["html"]
        haml             = test["haml"]
        locals           = Hash[(test["locals"] || {}).map {|x, y| [x.to_sym, y]}]
        options          = Hash[(test["config"] || {}).map {|x, y| [x.to_sym, y]}]
        options[:format] = options[:format].to_sym if options.key?(:format)
        engine           = CHaml::Engine.new(haml, options)
        locals.each{|x,y| scope.instance_eval("def #{x}; '#{y}'; end") }
        result           = engine.render(scope)

        assert_equal html, result.strip
      end
    end
  end
end
