#!/usr/bin/ruby 
# 
# A manual filter to generate links from the page catalog.
# 
# Authors:
# * Michael Granger <ged@FaerieMUD.org>
# * Mahlon E. Smith <mahlon@martini.nu>
# 
# 

### Avoid declaring the class if the tasklib hasn't been loaded yet.
unless Object.const_defined?( :Manual )
	raise LoadError, "not intended for standalone use: try the 'manual.rb' rake tasklib"
end



### A filter for generating links from the page catalog. This allows you to refer to other pages
### in the source and have them automatically updated as the structure of the manual changes.
### 
### Links are XML processing instructions. Pages can be referenced in one of several ways:
###
###   <?link Page Title ?>
###   <?link "click here":Page Title ?>
### 
### This first form links to a page by title. Link text defaults to the page title unless an 
### optional quoted string is prepended.
###
###   <?link path/to/Catalog.page ?>
###   <?link "click here":path/to/Catalog.page ?>
###
### The second form links to a page by its path relative to the base manual source directory.
### Again, the link text defaults to the page title, or can be overriden via a prepended string.
### 
class LinksFilter < Manual::Page::Filter
	
	# PI	   ::= '<?' PITarget (S (Char* - (Char* '?>' Char*)))? '?>'
	LinkPI = %r{
		<\?
			link				# Instruction Target
			\s+
			(?:"					
				(.*?)           # Optional link text [$1]
			":)?
			(.*?)				# Title or path [$2]
			\s+
		\?>
	  }x
	
	
	######
	public
	######

	### Process the given +source+ for <?link ... ?> processing-instructions, calling out
	def process( source, page, metadata )
		return source.gsub( LinkPI ) do |match|
			# Grab the tag values
			link_text = $1
			reference = $2
			
			self.generate_link( page, reference, link_text )
		end
	end
	
	
	### Create an HTML link fragment from the parsed LinkPI.
	###
	def generate_link( current_page, reference, link_text=nil )

		if other_page = self.find_linked_page( current_page, reference )
			href_path = other_page.sourcefile.relative_path_from( current_page.sourcefile.dirname )
			href = href_path.to_s.gsub( '.page', '.html' )
		
			if link_text
				return %{<a href="#{href}">#{link_text}</a>}
			else
				return %{<a href="#{href}">#{other_page.title}</a>}
			end
		else
			link_text ||= reference
			error_message = "Could not find a link for reference '%s'" % [ reference ]
			$stderr.puts( error_message )
			return %{<a href="#" title="#{error_message}" class="broken-link">#{link_text}</a>}
		end
	end
	
	
	### Lookup a page +reference+ in the catalog.  +reference+ can be either a
	### path to the .page file, relative to the manual root path, or a page title.
	### Returns a matching Page object, or nil if no match is found.
	###
	def find_linked_page( current_page, reference )
		
		catalog = current_page.catalog
		
		# Lookup by page path
		if reference =~ /\.page$/
			return catalog.uri_index[ reference ]
			
		# Lookup by page title
		else
			return catalog.title_index[ reference ]
		end
	end
end





