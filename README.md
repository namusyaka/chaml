# CHaml

Haml implementation written in C++.

**The project has been left unfinished.**

## Installation

Add this line to your application's Gemfile:

    gem 'chaml'

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install chaml

## Usage

### `parse`

```ruby
require 'chaml'

CHaml.parse(<<HAML)
%html
  %head
    %title hello
HAML
```

### `read`

```ruby
require 'chaml'

CHaml.read("/path/to/haml/template.haml")
```

## Contributing

1. Fork it
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create new Pull Request
