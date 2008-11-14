#!/usr/bin/ruby 
# 
# A collection of standard filters for the manual generation tasklib.
# $Id: examples-filter.rb 40 2008-08-27 21:58:00Z deveiant $
# 
# Authors:
#   Michael Granger <ged@FaerieMUD.org>
# 
# 

# Dependencies deferred until #initialize


### Avoid declaring the class if the tasklib hasn't been loaded yet.
unless Object.const_defined?( :Manual )
	raise LoadError, "not intended for standalone use: try the 'manual.rb' rake tasklib"
end



### A filter for inline example code or command-line sessions -- does
### syntax-highlighting (via CodeRay), syntax-checking for some languages, and
### captioning.
### 
### Examples are enclosed in XML processing instructions like so:
###
###   <?example {language: ruby, testable: true, caption: "A fine example"} ?>
###      a = 1
###      puts a
###   <?end example ?>
###
### This will be pulled out into a preformatted section in the HTML,
### highlighted as Ruby source, checked for valid syntax, and annotated with
### the specified caption. Valid keys in the example PI are:
### 
### language::
###   Specifies which (machine) language the example is in.
### testable::
###	  If set and there is a testing function for the given language, run it and append
###	  any errors to the output.
### caption::
###   A small blurb to put below the pulled-out example in the HTML.
class ExamplesFilter < Manual::Page::Filter
	
	DEFAULTS = {
		:language     => :ruby,
		:line_numbers => :inline,
		:tab_width    => 4,
		:hint         => :debug,
		:testable     => true
	}

	# PI	   ::= '<?' PITarget (S (Char* - (Char* '?>' Char*)))? '?>'
	ExamplePI = %r{
		<\?
			example				# Instruction Target
			(?:					# Optional instruction body
			\s+
			(					# [$1]
				[^?]*			# Anything but a question mark
				|				# -or-
				\?(?!>)			# question mark not followed by a closing angle bracket
			)
			)?
		\?>
	  }x
	
	EndPI = %r{ <\? end (?: \s+ example )? \s* \?> }x

	RENDERER_OPTIONS = YAML.load( File.read(__FILE__).split(/^__END__/, 2).last )
	

	### Defer loading of dependenies until the filter is loaded
	def initialize( *args )
		begin
			require 'pathname'
			require 'strscan'
			require 'yaml'
			require 'uv'
			require 'rcodetools/xmpfilter'
			require 'digest/md5'
			require 'tmpdir'
		rescue LoadError => err
			unless Object.const_defined?( :Gem )
				require 'rubygems'
				retry
			end

			raise
		end
	end
	
	
	######
	public
	######

	### Process the given +source+ for <?example ... ?> processing-instructions, calling out
	def process( source, page, metadata )
		scanner = StringScanner.new( source )
		
		buffer = ''
		until scanner.eos?
			startpos = scanner.pos
			
			# If we find an example
			if scanner.skip_until( ExamplePI )
				contents = ''
				
				# Append the interstitial content to the buffer
				if ( scanner.pos - startpos > scanner.matched.length )
					offset = scanner.pos - scanner.matched.length - 1
					buffer << scanner.string[ startpos..offset ]
				end

				# Append everything up to it to the buffer and save the contents of
				# the tag
				params = scanner[1]
				
				# Now find the end of the example or complain
				contentpos = scanner.pos
				scanner.skip_until( EndPI ) or
					raise "Unterminated example at line %d" % 
						[ scanner[0..scanner.pos].count("\n") ]
				
				# Now build the example and append to the buffer
				if ( scanner.pos - contentpos > scanner.matched.length )
					offset = scanner.pos - scanner.matched.length - 1
					contents = scanner.string[ contentpos..offset ]
				end

				#$stderr.puts "Processing with params: %p, contents: %p" % [ params, contents ]
				buffer << self.process_example( params, contents )
			else
				break
			end

		end
		buffer << scanner.rest
		scanner.terminate
		
		return buffer
	end
	
	
	### Filter out 'example' macros, doing syntax highlighting, and running
	### 'testable' examples through a validation process appropriate to the
	### language the example is in.
	def process_example( params, body )
		options = self.parse_options( params )
		caption = options.delete( :caption )
		content = ''
		
		# Test it if it's testable
		if options[:testable]
			content = test_content( body, options[:language] )
		else
			content = body
		end

		# Strip trailing blank lines and syntax-highlight
		content = highlight( content.strip, options )
		caption = %{<div class="caption">} + caption.to_s + %{</div>} if caption

		return %{<notextile><div class="example">%s%s</div></notextile>} %
		 	[content, caption || '']
	end


	### Parse an options hash for filtering from the given +args+, which can either 
	### be a plain String, in which case it is assumed to be the name of the language the example 
	### is in, or a Hash of configuration options.
	def parse_options( args )
		args = "{ #{args} }" unless args.strip[0] == ?{
		args = YAML.load( args )

		# Convert to Symbol keys and value
		args.keys.each do |k|
			newval = args.delete( k )
			next if newval.nil? || (newval.respond_to?(:size) && newval.size == 0)
			args[ k.to_sym ] = newval.respond_to?( :to_sym ) ? newval.to_sym : newval
		end
		return DEFAULTS.merge( args )
	end
	

	### Test the given +content+ with a rule specific to the given +language+.
	def test_content( body, language )
		case language.to_sym
		when :ruby
			return self.test_ruby_content( body )

		when :yaml
			return self.test_yaml_content( body )

		else
			return body
		end
	end
	
		
	### Test the specified Ruby content for valid syntax
	def test_ruby_content( source )
		# $stderr.puts "Testing ruby content..."
		libdir = Pathname.new( __FILE__ ).dirname.parent.parent.parent + 'lib'

		options = Rcodetools::XMPFilter::INITIALIZE_OPTS.dup
		options[:include_paths] << libdir.to_s

		rval = Rcodetools::XMPFilter.run( source, options )

		# $stderr.puts "test output: ", rval
		return rval.join
	rescue Exception => err
		return "%s while testing %s: %s\n  %s" %
			[ err.class.name, sourcefile, err.message, err.backtrace.join("\n  ") ]
	end
	
	
	### Test the specified YAML content for valid syntax
	def test_yaml_content( source )
		YAML.load( source )
	rescue YAML::Error => err
		return "# Invalid YAML: " + err.message + "\n" + source
	else
		return source
	end
	
	
	### Work around Ultraviolet's retarded interface
	def parse_with_ultraviolet( content, lang )
		Uv.init_syntaxes
		syntaxes = Uv.instance_variable_get( :@syntaxes )
		
		processor = Uv::RenderProcessor.new( RENDERER_OPTIONS, true, false )
		syntaxes[ lang ].parse( content, processor )

		return processor.string
	end
	

	### Highlights the given +content+ in language +lang+.
	def highlight( content, options )
		lang = options.delete( :language ).to_s
		if Uv.syntaxes.include?( lang )
			return parse_with_ultraviolet( content, lang )
		else
			begin
				require 'amatch'
				pat = Amatch::PairDistance.new( lang )
				matches = Uv.syntaxes.
					collect {|syntax| [pat.match(syntax), syntax] }.
					sort_by {|tuple| tuple[0] }.
					reverse
				puts matches[ 0..5 ].inspect
				puts "No syntax called '#{lang}'.",
					"Perhaps you meant one of: ",
					*(matches[ 0..5 ].collect {|m| "  #{m[1]}" })
			rescue => err
				$stderr.puts err.message, err.backtrace.join("\n  ")
				raise "No UV syntax called '#{lang}'."
			end
		end
	end
	
end


__END__
--- 
name: DevEiate
line: 
  begin: ""
  end: ""
tags: 
- begin: <span class="Comment">
  end: </span>
  selector: comment
- begin: <span class="Constant">
  end: </span>
  selector: constant
- begin: <span class="Entity">
  end: </span>
  selector: entity
- begin: <span class="Keyword">
  end: </span>
  selector: keyword
- begin: <span class="Storage">
  end: </span>
  selector: storage
- begin: <span class="String">
  end: </span>
  selector: string
- begin: <span class="Support">
  end: </span>
  selector: support
- begin: <span class="Variable">
  end: </span>
  selector: variable
- begin: <span class="InvalidDeprecated">
  end: </span>
  selector: invalid.deprecated
- begin: <span class="InvalidIllegal">
  end: </span>
  selector: invalid.illegal
- begin: <span class="RubyInstanceVariable">
  end: </span>
  selector: variable.other.readwrite.instance.ruby
- begin: <span class="RubyConstant">
  end: </span>
  selector: variable.other.constant.ruby
- begin: <span class="RubyClass">
  end: </span>
  selector: entity.name.class.ruby, entity.name.class.module.ruby
- begin: <span class="RubyInheritedClass">
  end: </span>
  selector: entity.other.inherited-class.ruby, entity.other.inherited-class.module.ruby
- begin: <span class="EmbeddedSource">
  end: </span>
  selector: text source
- begin: <span class="EmbeddedSourceBright">
  end: </span>
  selector: text.html.ruby source
- begin: <span class="EntityInheritedClass">
  end: </span>
  selector: entity.other.inherited-class
- begin: <span class="StringEmbeddedSource">
  end: </span>
  selector: string.quoted source
- begin: <span class="StringConstant">
  end: </span>
  selector: string constant
- begin: <span class="StringRegexp">
  end: </span>
  selector: string.regexp
- begin: <span class="StringRegexpSpecial">
  end: </span>
  selector: string.regexp constant.character.escaped, string.regexp source.ruby.embedded, string.regexp string.regexp.arbitrary-repitition
- begin: <span class="StringVariable">
  end: </span>
  selector: string variable
- begin: <span class="SupportFunction">
  end: </span>
  selector: support.function
- begin: <span class="SupportConstant">
  end: </span>
  selector: support.constant
- begin: <span class="CCCPreprocessorLine">
  end: </span>
  selector: other.preprocessor.c
- begin: <span class="CCCPreprocessorDirective">
  end: </span>
  selector: other.preprocessor.c entity
- begin: <span class="DoctypeXmlProcessing">
  end: </span>
  selector: declaration.sgml.html declaration.doctype, declaration.sgml.html declaration.doctype entity, declaration.sgml.html declaration.doctype string, declaration.xml-processing, declaration.xml-processing entity, declaration.xml-processing string, meta.tag.preprocessor.xml
- begin: <span class="XmlPreprocessingDirective">
  end: </span>
  selector: meta.tag.preprocessor.xml entity.name.tag.xml
- begin: <span class="XmlPreprocessingAttributes">
  end: </span>
  selector: meta.tag.preprocessor.xml entity.other.attribute-name.xml
- begin: <span class="MetaTagAll">
  end: </span>
  selector: declaration.tag, declaration.tag entity, meta.tag, meta.tag entity
- begin: <span class="MetaTagInline">
  end: </span>
  selector: declaration.tag.inline, declaration.tag.inline entity, source entity.name.tag, source entity.other.attribute-name, meta.tag.inline, meta.tag.inline entity
- begin: <span class="CssTagName">
  end: </span>
  selector: meta.selector.css entity.name.tag
- begin: <span class="CssPseudoClass">
  end: </span>
  selector: meta.selector.css entity.other.attribute-name.tag.pseudo-class
- begin: <span class="CssId">
  end: </span>
  selector: meta.selector.css entity.other.attribute-name.id
- begin: <span class="CssClass">
  end: </span>
  selector: meta.selector.css entity.other.attribute-name.class
- begin: <span class="CssPropertyName">
  end: </span>
  selector: support.type.property-name.css
- begin: <span class="CssPropertyValue">
  end: </span>
  selector: meta.property-group support.constant.property-value.css, meta.property-value support.constant.property-value.css
- begin: <span class="CssAtRule">
  end: </span>
  selector: meta.preprocessor.at-rule keyword.control.at-rule
- begin: <span class="CssAdditionalConstants">
  end: </span>
  selector: meta.property-value support.constant.named-color.css, meta.property-value constant
- begin: <span class="CssConstructorArgument">
  end: </span>
  selector: meta.constructor.argument.css
- begin: <span class="DiffHeader">
  end: </span>
  selector: meta.diff, meta.diff.header
- begin: <span class="DiffDeleted">
  end: </span>
  selector: markup.deleted
- begin: <span class="DiffChanged">
  end: </span>
  selector: markup.changed
- begin: <span class="DiffInserted">
  end: </span>
  selector: markup.inserted
- begin: <span class="MarkupList">
  end: </span>
  selector: markup.list
- begin: <span class="MarkupHeading">
  end: </span>
  selector: markup.heading
- begin: <span class="WebgenMetadataHeader">
  end: </span>
  selector: text.html.textile.webgen.metadata
- begin: <span class="WebgenPlugin">
  end: </span>
  selector: text.html.textile.webgen.plugin
- begin: <span class="WebgenPluginParameters">
  end: </span>
  selector: text.html.textile.webgen.plugin-parameters
- begin: <span class="EvenTabs">
  end: </span>
  selector: meta.even-tab
listing: 
  begin: <pre class="deveiate">
  end: </pre>
document: 
  begin: ""
  end: ""
filter: CGI.escapeHTML( @escaped )
line-numbers: 
  begin: <span class="line-numbers">
  end: </span>
