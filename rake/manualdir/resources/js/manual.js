
// Auto generate a table of contents if a #auto-toc div exists.
//
$(document).ready(function() {
	
	$('#auto-toc').wrapInner('<ul></ul>');
		
	$('h3').each( function() {
		var header = $(this);
		var html = header.html().replace( /^\s*|\s*$/g, '' );
		var newid = html.toLowerCase().replace( /\W+/g, '-' );
		header.prepend( '<a name="' + newid + '" />' );
		
		$('#auto-toc ul').append( '<li><a href="#' + newid + '">' + html + "</a></li>" );
	});
});