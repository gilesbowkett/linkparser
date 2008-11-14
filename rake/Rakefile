#!rake
#
# Rake-TaskLibs rakefile
# 
# Copyright (c) 2008 The FaerieMUD Consortium
#
# Authors:
#  * Michael Granger <ged@FaerieMUD.org>
#

BEGIN {
	require 'pathname'
	basedir = Pathname.new( __FILE__ ).dirname

	libdir = basedir + "lib"
	extdir = basedir + "ext"

	$LOAD_PATH.unshift( libdir.to_s ) unless $LOAD_PATH.include?( libdir.to_s )
	$LOAD_PATH.unshift( extdir.to_s ) unless $LOAD_PATH.include?( extdir.to_s )
}


require 'rbconfig'
require 'rubygems'
require 'rake'
require 'rake/clean'

$dryrun = false

### Config constants
BASEDIR       = Pathname.new( __FILE__ ).dirname.relative_path_from( Pathname.getwd )
RAKE_TASKDIR  = BASEDIR

PKG_NAME      = 'rake-tasklibs'
PKG_SUMMARY   = ''
VERSION_FILE  = BASEDIR + 'Metarakefile'
PKG_VERSION   = VERSION_FILE.read[ /VERSION = '(\d+\.\d+\.\d+)'/, 1 ]
PKG_FILE_NAME = "#{PKG_NAME.downcase}-#{PKG_VERSION}"
GEM_FILE_NAME = "#{PKG_FILE_NAME}.gem"

# Subversion constants -- directory names for releases and tags
SVN_TRUNK_DIR    = 'trunk'
SVN_RELEASES_DIR = 'releases'
SVN_BRANCHES_DIR = 'branches'
SVN_TAGS_DIR     = 'tags'

### Load some task libraries that need to be loaded early
require RAKE_TASKDIR + 'helpers.rb'
require RAKE_TASKDIR + 'svn.rb'

$trace = Rake.application.options.trace ? true : false
$dryrun = Rake.application.options.dryrun ? true : false


#####################################################################
###	T A S K S 	
#####################################################################

### Default task
task :default do
	error_message "You probably meant to run the Metarakefile."
	ask_for_confirmation( "Want me to switch over to it now?" ) do
		mrf = BASEDIR + 'Metarakefile'
		exec 'rake', '-f', mrf, *ARGV
	end
end

# Stub out the testing task, as there aren't currently any tests
task :test

