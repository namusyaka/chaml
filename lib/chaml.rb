require "chaml/version"
require "chaml/engine"

module CHaml
  # Constructs an instance of CHaml::Engine
  # @param template [String] The Haml template
  # @param options [Hash] An options hash
  # @return [CHaml::Engine] An instance of CHaml::Engine
  def self.parse(template, options = {})
    CHaml::Engine.new(template, options)
  end

  # Reads string as a haml template, and passes it to CHaml::Engine
  # @param path [String] A path of the haml template
  # @return [CHaml::Engine]
  def self.read(path)
    CHaml.parse(File.read(path))
  end
end
