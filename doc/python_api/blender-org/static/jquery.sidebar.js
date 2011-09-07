$(document).ready(function () {
    var top = $('.sphinxsidebarwrapper').offset().top - parseFloat($('.sphinxsidebarwrapper').css ('marginTop').replace(/auto/, 0));
    var colheight = parseFloat($('.sphinxsidebarwrapper').css('height').replace(/auto/, 0));


$(window).scroll(function (event) {
    // what the y position of the scroll is
    var y = $(this).scrollTop();

    // whether that's below the form
    if (y >= top) {
        //colheight is checked and according to its vaule the scrolling
        //is triggered or not
        if (colheight <= window.innerHeight) {
            // if so, ad the fixed class
            $('.sphinxsidebarwrapper').addClass('fixed');
        } else {
            // otherwise remove it
            $('.sphinxsidebarwrapper').removeClass('fixed');
        }
    } else {
        // otherwise remove it
        $('.sphinxsidebarwrapper').removeClass('fixed');
    }
});
}); 
