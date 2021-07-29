*************************
Contributing small things
*************************

If you're reading this because you would like to change or add code to sverchok, that's cool. If not, you can skip this document now.

Thanks for taking the time to read on, we gladly accept contributions to our code and documentation. We will always consider the proposed changes in light of our understanding of sverchok as a whole. Sverchok's code base is large and in a few places challenging to comprehend. If we don't think it's appropriate to accept your suggestion we'll defend our position. Usually we can come to some compromise that makes sense for sverchok, yet also satisfies the amicable contributor.


Pull Requests
=============

A pull request (PR) is what you might consider a "formally suggested edit" to sverchok. You do this either via the online Git tools provided by GitHub / or via GIT. Newcomers to git should probably use the online GitHub mechanisms for doing small edits. See GitHub's guide to PRs ( https://help.github.com/articles/about-pull-requests/ ).

We accept one-liner PRs, but might also reject them and instead add the same (or similar) code modifications to our own frequent PR streaks. We'll usually add a comment in the commit to acknowledge your input.


Informal code suggestions
=========================

If you don't want to go through the git / github interface you can suggest an edit using the issue tracker. Open a new issue, state succintly in the title what the edit hopes to achieve. In the issue you can use Markdown to show us which file and section of code you're talking about and what changes you think would be beneficial (and why). You can copy / paste a permalink ( https://github.com/blog/2415-introducing-embedded-code-snippets ) into the issue for this purpose.

And below it write the following "differential". 

.. code-block :: none

    In the following file: sverchok/nodes/scene/frame_info.py on line 122


       ```diff
       - print(some_variable)
       + # print(some_variable)
       ```

    this node prints very often, and seems to slow down Blender because of it. 
    I'm not certain but I don't think this printout it essential and suggest you drop it.


using the backticks and the diff (language type) it will display the proposed change in code with syntax highlighting like this:

.. code-block :: diff

   - print(some_variable)
   + # print(some_variable)


This way you show what code you're talking about, what you propose to change, and why. Great. We love clear issues.


Non one-liners
==============

Sometimes a small contribution is 2 or more lines. It's hard to define where the term "small contribtion" becomes inaccurate to describe your proposal. We'll know it when we see it. We also accept code block rewrites or function replacements. The more you propose to change the more we will want to see profiling/performance evidence to defend your claim. 

What gets our attention
=======================

Everything. We tend to be more lenient/rapid if your code proposal fixes a real flaw in existing code, and doesn't involve changing too much outside of the area in question.


General guidelines
==================

The document you should be reading to get a sense of how we'd like contributed code to look and behave is here ( contribute.rst ).
