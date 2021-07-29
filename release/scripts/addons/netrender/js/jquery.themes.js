/* http://keith-wood.name/themes.html
   Applying CSS themes for jQuery v1.2.0.
   Written by Keith Wood (kbwood{at}iinet.com.au) September 2008.
   Dual licensed under the GPL (http://dev.jquery.com/browser/trunk/jquery/GPL-LICENSE.txt) and 
   MIT (http://dev.jquery.com/browser/trunk/jquery/MIT-LICENSE.txt) licenses. 
   Please attribute the author if you use it. */

/* Apply various themes to your site.
   Attach the functionality with options like:
   $('link').themes({themes: ['cupertino', 'smoothness']});
*/

(function($) { // Hide scope, no $ conflict

var COOKIE_NAME = 'themeID';

/* Theme sharing manager. */
function Themes() {
	this.currentTheme = '';
	this._uuid = new Date().getTime();
	this._linkID = 'th' + this._uuid;
	this._defaults = {
		themeBase: '',  // The base URL for all the theme files
		defaultTheme: '', // The ID of the default theme, first one if blank
		cookieExpiry: 0,  // The expiry time for the theme cookie, date or number of days
		cookiePath: '/',  // The path that the cookie applies to
		cookieDomain: '',  // The domain that the cookie applies to
		onSelect: null  // Callback on theme selection, theme ID and URL are passed as parameters
	};
	this._settings = {
		themes: [],  // List of theme IDs to use, empty for all
		icons: 'img/themes.gif', // Horizontal amalgamation of all theme icons
		iconSize: [23, 20],  // The width and height of the individual icons
		previews: 'img/themes-preview.gif', // Horizontal amalgamation of all theme previews
		previewSize: [90, 80],  // The width and height of the individual previews
		showPreview: true,  // True to display a popup preview, false to not show it
		compact: true  // True if a compact presentation should be used, false for full
	};
	this._themes = {  // The definitions of the available themes
		'blacktie': {display: 'Black Tie', icon: 0, preview: 0,
			url: 'black-tie/ui.all.css'},
		'blitzer': {display: 'Blitzer', icon: 1, preview: 1,
			url: 'blitzer/ui.all.css'},
		'cupertino': {display: 'Cupertino', icon: 2, preview: 2,
			url: 'cupertino/ui.all.css'},
		'darkhive': {display: 'Dark Hive', icon: 17, preview: 17,
			url: 'dark-hive/ui.all.css'},
		'dotluv': {display: 'Dot Luv', icon: 3, preview: 3,
			url: 'dot-luv/ui.all.css'},
		'eggplant': {display: 'Eggplant', icon: 18, preview: 18,
			url: 'eggplant/ui.all.css'},
		'excitebike': {display: 'Excite Bike', icon: 4, preview: 4,
			url: 'excite-bike/ui.all.css'},
		'flick': {display: 'Flick', icon: 19, preview: 19,
			url: 'flick/ui.all.css'},
		'hotsneaks': {display: 'Hot Sneaks', icon: 5, preview: 5,
			url: 'hot-sneaks/ui.all.css'},
		'humanity': {display: 'Humanity', icon: 6, preview: 6,
			url: 'humanity/ui.all.css'},
		'lefrog': {display: 'Le Frog', icon: 20, preview: 20,
			url: 'le-frog/ui.all.css'},
		'mintchoc': {display: 'Mint Choc', icon: 7, preview: 7,
			url: 'mint-choc/ui.all.css'},
		'overcast': {display: 'Overcast', icon: 21, preview: 21,
			url: 'overcast/ui.all.css'},
		'peppergrinder': {display: 'Pepper Grinder', icon: 22, preview: 22,
			url: 'pepper-grinder/ui.all.css'},
		'redmond': {display: 'Redmond', icon: 8, preview: 8,
			url: 'redmond/ui.all.css'},
		'smoothness': {display: 'Smoothness', icon: 9, preview: 9,
			url: 'smoothness/ui.all.css'},
		'southstreet': {display: 'South Street', icon: 10, preview: 10,
			url: 'south-street/ui.all.css'},
		'start': {display: 'Start', icon: 11, preview: 11,
			url: 'start/ui.all.css'},
		'sunny': {display: 'Sunny', icon: 23, preview: 23,
			url: 'sunny/ui.all.css'},
		'swankypurse': {display: 'Swanky Purse', icon: 12, preview: 12,
			url: 'swanky-purse/ui.all.css'},
		'trontastic': {display: 'Trontastic', icon: 13, preview: 13,
			url: 'trontastic/ui.all.css'},
		'uidarkness': {display: 'UI Darkess', icon: 14, preview: 14,
			url: 'ui-darkness/ui.all.css'},
		'uilightness': {display: 'UI Lightness', icon: 15, preview: 15,
			url: 'ui-lightness/ui.all.css'},
		'vader': {display: 'Vader', icon: 16, preview: 16,
			url: 'vader/ui.all.css'}
	};
}

$.extend(Themes.prototype, {
	/* Class name added to elements to indicate already configured with themeing. */
	markerClassName: 'hasThemes',

	/* Override the default settings for all theme instances.
	   @param  settings  (object) the new settings to use as defaults
	   @return  (Theme object) this object */
	setDefaults: function(settings) {
		extendRemove(this._defaults, settings || {});
		return this;
	},

	/* Initialise the theme for the page.
	   @param  settings  (object, optional) default settings to be used
	   @return  (Theme object) this object */
	init: function(settings) {
		if (settings) {
			this.setDefaults(settings);
		}
		var search = new RegExp(COOKIE_NAME + '=([^;]*)');
		var matches = search.exec(document.cookie);
		var themeId = (matches ? matches[1] : '') || this._defaults.defaultTheme;
		var firstId = '';
		var found = false;
		for (var id in this._themes) {
			firstId = firstId || id;
			found = (themeId == id);
			if (found) {
				break;
			}
		}
		themeId = (found ? themeId : firstId);
		this._setTheme(themeId, this._themes[themeId].display,
			this._defaults.themeBase + this._themes[themeId].url, true);
		return this;
	},

	/* Add a new theme to the list.
	   @param  id       (string) the ID of the new theme
	   @param  display  (string) the display name for this theme
	   @param  icon     (url) the location of an icon for this theme (25x22), or
	                    (number) the index of the icon within the combined image
	   @param  preview  (url) the location of a preview for this theme (105x92), or
	                    (number) the index of the preview within the combined image
	   @param  url      (url) the URL for the theme definition (CSS)
	   @return  (Theme object) this object */
	addTheme: function(id, display, icon, preview, url) {
		this._themes[id] = {display: display, icon: icon, preview: preview, url: url};
		return this;
	},

	/* Return the list of defined themes.
	   @return  (object[]) indexed by theme id (string), each object contains
	            display (string) the display name,
	            icon    (string) the location of the icon,, or
	                    (number) the icon's index in the combined image
	            url     (string) the URL for the theme's CSS definition */
	getThemes: function() {
		return this._themes;
	},

	/* Attach the themeing widget to a div.
	   @param  target    (element) the element to attach to
	   @param  settings  (object) the settings for this element */
	_attachThemes: function(target, settings) {
		target = $(target);
		if (target.hasClass(this.markerClassName)) {
			return;
		}
		target.addClass(this.markerClassName);
		this._updateTheme(target, settings);
	},

	/* Reconfigure the settings for a theme link.
	   @param  target    (element) the element to change settings for
	   @param  settings  (object) the changed settings for this element */
	_changeThemes: function(target, settings) {
		target = $(target);
		if (!target.hasClass(this.markerClassName)) {
			return;
		}
		this._updateTheme(target, settings);
	},

	/* Construct the requested themeing links.
	   @param  target    (element) the element to attach to
	   @param  settings  (object) the settings for this element */
	_updateTheme: function(target, settings) {
		settings = extendRemove($.extend({}, this._settings, this._defaults), settings);
		var themes = settings.themes;
		if (themes.length == 0) {
			$.each(this._themes, function(id) {
				themes[themes.length] = id;
			});
		}
		var escape = function(value) {
			return value.replace(/'/, '\\\'');
		};
		var previewId = 'th' + ++this._uuid;
		var html = '<ul class="themes_list' + (settings.compact ? ' themes_compact' : '') + '">';
		var allThemes = this._themes;
		$.each(themes, function(index, id) {
			var theme = allThemes[id];
			if (!theme) {
				return;
			}
			html += '<li class="themes__' + id + ($.themes.currentTheme == id ? ' themes_current' : '') +
				'"><a onclick="$.themes._setTheme(\'' + id + '\',\'' +
				escape(theme.display) + '\',\'' + escape(settings.themeBase + theme.url) + '\')"' +
				(!settings.showPreview ? '' :
				' onmouseover="$.themes._showPreview(this, \'#' + previewId + '\',\'' +
				escape(theme.display) + '\',' + (typeof theme.preview == 'number' ? theme.preview :
				'\'' + theme.preview + '\'') + ',' + settings.previewSize[0] + ')" ' +
				'onmouseout="$.themes._hidePreview(\'#' + previewId + '\')"') + '>';
			if (theme.icon != null) {
				html += '<span title="' + theme.display + '"';
				if (typeof theme.icon == 'number') {
					html += ' style="background: transparent url(' + settings.icons +
						') no-repeat -' + (theme.icon * settings.iconSize[0]) + 'px 0px;' +
						($.browser.mozilla && $.browser.version < '1.9' ?
						' padding-left: ' + settings.iconSize[0] + 'px;' +
						' padding-bottom: ' + (settings.iconSize[1] - 16) + 'px;' : '') + '">';
				}
				else {
					html += '><img src="' + theme.icon + '" alt="' + theme.display + '"/>';
				}
				html +=	'</span>' + (settings.compact ? '' : '&#xa0;');
			}
			html +=	(settings.compact ? '' : theme.display) + '</a></li>';
		});
		html += '</ul><div id="' + previewId + '" class="themes_preview"><div ' +
			'style="width: ' + settings.previewSize[0] + 'px; height: ' + settings.previewSize[1] +
			'px; background: url(' + settings.previews + ') no-repeat;"></div></div>';
		target.html(html);
	},

	/* Remove the themeing widget from a link. */
	_destroyThemes: function(target) {
		target = $(target);
		if (!target.hasClass(this.markerClassName)) {
			return;
		}
		target.removeClass(this.markerClassName).empty();
	},

	/* Load a new theme.
	   @param  id       (string) the ID of the new theme
	   @param  display  (string) the display name of the new theme
	   @param  url      (string) the location of the CSS
	   @param  loading  (boolean) true if initially loading */
	_setTheme: function(id, display, url, loading) {
		if ($('#' + $.themes._linkID).length == 0) {
			$('head').append('<link rel="stylesheet" type="text/css" id="' + $.themes._linkID + '"/>');
		}
		$('#' + $.themes._linkID).attr('href', url);
		$.themes.currentTheme = id;
		$('.' + $.themes.markerClassName + ' li').removeClass('themes_current');
		$('.themes__' + id).addClass('themes_current');
		// Custom callback
		var onSelect = $.themes._defaults.onSelect;
		if (onSelect) {
			onSelect.apply(window, [id, display, url]);
		}
		if (!loading) {
			// Calculate cookie expiry
			var addDays = function(days) {
				var date = new Date();
				date.setDate(date.getDate() + days);
				return date;
			};
			var expiryDate = ($.themes._defaults.cookieExpiry ?
				(typeof $.themes._defaults.cookieExpiry == 'number' ?
				addDays($.themes._defaults.cookieExpiry) : $.themes._defaults.cookieExpiry) : null);
			// Save theme setting as cookie
			document.cookie = COOKIE_NAME + '=' + id +
				(expiryDate ? '; expires=' + expiryDate.toUTCString() : '') +
				($.themes._defaults.cookiePath ? '; path=' + $.themes._defaults.cookiePath : '') +
				($.themes._defaults.cookieDomain ? '; domain=' + $.themes._defaults.cookieDomain : '');
		}
		return false;
	},

	/* Show the preview for the current theme.
	   @param  link     (element) the element containing the theme link
	   @param  id       (string) the ID of the preview division
	   @param  display  (string) the display text for this theme
	   @param  preview  (string) the URL of the preview image or
	                    (number) index into the combined previews
	   @param  width    (number) the width of the preview images */
	_showPreview: function(link, id, display, preview, width) {
		if (preview == null) {
			return;
		}
		var html = '';
		if (typeof preview == 'number') {
			$(id + ' div').show().
				css('background-position', '-' + (preview * width) + 'px 0px');
		}
		else {
			$(id + ' div').hide();
			html += '<img src="' + preview + '" alt="' + display + '"/><br/>';
		}
		html += '<span>' + display + '</span>';
		var parent = $(link).parent();
		var absParent = null;
		$(link).parents().each(function() {
			if ($(this).css('position') == 'absolute') {
				absParent = $(this);
			}
			return !absParent;
		});
		var offset = parent.offset();
		var absOffset = (absParent ? absParent.offset() : {left: 0, top: 0});
		var left = offset.left - absOffset.left + parseInt(parent.css('padding-left'), 10);
		var top = offset.top - absOffset.top + parent.height() +
			parseInt(parent.css('padding-top'), 10) + 1;
		$(id).children(':not(:first)').remove().end().
			append(html).css({left: left, top: top}).show();
	},

	/* Hide the theme preview.
	   @param  id  (string) the ID of the preview division */
	_hidePreview: function(id) {
		$(id).hide();
	}
});

/* jQuery extend now ignores nulls! */
function extendRemove(target, props) {
	$.extend(target, props);
	for (var name in props) {
		if (props[name] == null) {
			target[name] = null;
		}
	}
	return target;
}

/* Attach the themeing functionality to a jQuery selection.
   @param  command  (string) the command to run (optional, default 'attach')
   @param  options  (object) the new settings to use for these themeing instances
   @return  (jQuery object) for chaining further calls */
$.fn.themes = function(options) {
	var otherArgs = Array.prototype.slice.call(arguments, 1);
	return this.each(function() {
		if (typeof options == 'string') {
			$.themes['_' + options + 'Themes'].
				apply($.themes, [this].concat(otherArgs));
		}
		else {
			$.themes._attachThemes(this, options || {});
		}
	});
};

/* Initialise the themeing functionality. */
$.themes = new Themes(); // singleton instance

})(jQuery);
