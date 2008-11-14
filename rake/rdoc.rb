# 
# RDoc Rake tasks for ThingFish
# $Id: rdoc.rb 40 2008-08-27 21:58:00Z deveiant $
# 

require 'rake/rdoctask'
$have_darkfish = false

# Append docs/lib to the load path if it exists for a locally-installed Darkfish
DOCSLIB = DOCSDIR + 'lib'
$LOAD_PATH.unshift( DOCSLIB.to_s ) if DOCSLIB.exist?

begin
	require 'darkfish-rdoc'
	$have_darkfish = true
rescue LoadError => err
	unless Object.const_defined?( :Gem )
		require 'rubygems'
		gem 'darkfish-rdoc'
		retry
	end
	
	log "No DarkFish: %s: %s" % [ err.class.name, err.message ]
	trace "Backtrace:\n  %s" % [ err.backtrace.join("\n  ") ]
end

Rake::RDocTask.new do |rdoc|
	rdoc.rdoc_dir = RDOCDIR.expand_path.relative_path_from( Pathname.pwd ).to_s
	rdoc.title    = "#{PKG_NAME} - #{PKG_SUMMARY}"
	rdoc.options += RDOC_OPTIONS + [ '-f', 'darkfish' ] if $have_darkfish

	rdoc.rdoc_files.include 'README'
	rdoc.rdoc_files.include LIB_FILES.collect {|f| f.to_s }
	rdoc.rdoc_files.include EXT_FILES.collect {|f| f.to_s }
end
