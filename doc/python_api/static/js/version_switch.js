(function () {
  // switch: v1.2
  'use strict'

  const versionsFileUrl = 'https://docs.blender.org/PROD/versions.json'

  let all_versions

  const Popover = (function () {
    function Popover (id) {
      this.isOpen = false
      this.type = id === 'version-popover'
      this.$btn = $('#' + id)
      this.$dialog = this.$btn.next()
      this.$list = this.$dialog.children('ul')
      this.sel = null
      this.beforeInit()
    }

    Popover.prototype = {
      beforeInit: function () {
        const that = this
        this.$btn.on('click', function (e) {
          that.init()
          e.preventDefault()
          e.stopPropagation()
        })
        this.$btn.on('keydown', function (e) {
          if (that.btnKeyFilter(e)) {
            that.init()
            e.preventDefault()
            e.stopPropagation()
          }
        })
      },
      init: function () {
        this.$btn.off('click')
        this.$btn.off('keydown')

        if (all_versions === undefined) {
          this.$btn.addClass('wait')
          this.loadVL(this)
        } else {
          this.afterLoad()
        }
      },
      loadVL: function (that) {
        $.getJSON(versionsFileUrl, function (data) {
          all_versions = data
          that.afterLoad()
          return true
        }).fail(function () {
          that.$btn.addClass('disabled')
          return false
        })
      },
      afterLoad: function () {
        let release = DOCUMENTATION_OPTIONS.VERSION
        const m = release.match(/\d\.\d+/g)
        if (m) {
          release = m[0]
        }

        this.warnOld(release, all_versions)

        const version = this.getNamed(release)
        const list = this.buildList(version)

        this.$list.children(':first-child').remove()
        this.$list.append(list)
        const that = this
        this.$list.on('keydown', function (e) {
          that.keyMove(e)
        })

        this.$btn.removeClass('wait')
        this.btnOpenHandler()
        this.$btn.on('mousedown', function (e) {
          that.btnOpenHandler()
          e.preventDefault()
        })
        this.$btn.on('keydown', function (e) {
          if (that.btnKeyFilter(e)) {
            that.btnOpenHandler()
          }
        })
      },
      warnOld: function (release, all_versions) {
        // Note this is effectively disabled now, two issues must fixed:
        // * versions.js does not contain a current entry, because that leads to
        //   duplicate version numbers in the menu. These need to be deduplicated.
        // * It only shows the warning after opening the menu to switch version
        //   when versions.js is loaded. This is too late to be useful.
        let current = all_versions.current
        if (!current) {
          // console.log("Version Switch Error: no 'current' in version.json.");
          return
        }
        const m = current.match(/\d\.\d+/g)
        if (m) {
          current = parseFloat(m[0])
        }
        if (release < current) {
          const currentURL = window.location.pathname.replace(release, current)
          const warning = $(
            '<div class="admonition warning"> ' +
              '<p class="first admonition-title">Note</p> ' +
              '<p class="last"> ' +
              'You are not using the most up to date version of the documentation. ' +
              '<a href="#"></a> is the newest version.' +
              '</p>' +
              '</div>'
          )

          warning.find('a').attr('href', currentURL).text(current)

          let body = $('div.body')
          if (!body.length) {
            body = $('div.document')
          }
          body.prepend(warning)
        }
      },
      buildList: function (v) {
        const url = new URL(window.location.href)
        const pathSplit = ['', 'api', v]
        if (url.pathname.startsWith('/api/')) {
          pathSplit.push(url.pathname.split('/').slice(3).join('/'))
        } else {
          pathSplit.push(url.pathname.substring(1))
        }
        if (this.type) {
          var dyn = all_versions
          var cur = v
        }
        const buf = []
        const that = this
        $.each(dyn, function (ix, title) {
          buf.push('<li')
          if (ix === cur) {
            buf.push(
              ' class="selected" tabindex="-1" role="presentation"><span tabindex="-1" role="menuitem" aria-current="page">' +
                title +
                '</spanp></li>'
            )
          } else {
            pathSplit[2 + that.type] = ix
            const href = new URL(url)
            href.pathname = pathSplit.join('/')
            buf.push(
              ' tabindex="-1" role="presentation"><a href ="' +
                href +
                '" tabindex="-1">' +
                title +
                '</a></li>'
            )
          }
        })
        return buf.join('')
      },
      getNamed: function (v) {
        $.each(all_versions, function (ix, title) {
          if (ix === 'master' || ix === 'main' || ix === 'latest') {
            const m = title.match(/\d\.\d[\w\d\.]*/)[0]
            if (parseFloat(m) == v) {
              v = ix
              return false
            }
          }
        })
        return v
      },
      dialogToggle: function (speed) {
        const wasClose = !this.isOpen
        const that = this
        if (!this.isOpen) {
          this.$btn.addClass('version-btn-open')
          this.$btn.attr('aria-pressed', true)
          this.$dialog.attr('aria-hidden', false)
          this.$dialog.fadeIn(speed, function () {
            that.$btn.parent().on('focusout', function (e) {
              that.focusoutHandler()
              e.stopImmediatePropagation()
            })
            that.$btn.parent().on('mouseleave', function (e) {
              that.mouseoutHandler()
              e.stopImmediatePropagation()
            })
          })
          this.isOpen = true
        } else {
          this.$btn.removeClass('version-btn-open')
          this.$btn.attr('aria-pressed', false)
          this.$dialog.attr('aria-hidden', true)
          this.$btn.parent().off('focusout')
          this.$btn.parent().off('mouseleave')
          this.$dialog.fadeOut(speed, function () {
            if (this.$sel) {
              this.$sel.attr('tabindex', -1)
            }
            that.$btn.attr('tabindex', 0)
            if (
              document.activeElement !== null &&
              document.activeElement !== document &&
              document.activeElement !== document.body
            ) {
              that.$btn.focus()
            }
          })
          this.isOpen = false
        }

        if (wasClose) {
          if (this.$sel) {
            this.$sel.attr('tabindex', -1)
          }
          if (
            document.activeElement !== null &&
            document.activeElement !== document &&
            document.activeElement !== document.body
          ) {
            const $nw = this.listEnter()
            $nw.attr('tabindex', 0)
            $nw.focus()
            this.$sel = $nw
          }
        }
      },
      btnOpenHandler: function () {
        this.dialogToggle(300)
      },
      focusoutHandler: function () {
        const list = this.$list
        const that = this
        setTimeout(function () {
          if (list.find(':focus').length === 0) {
            that.dialogToggle(200)
          }
        }, 200)
      },
      mouseoutHandler: function () {
        this.dialogToggle(200)
      },
      btnKeyFilter: function (e) {
        if (e.ctrlKey || e.shiftKey) {
          return false
        }
        if (
          e.key === ' ' ||
          e.key === 'Enter' ||
          (e.key === 'ArrowDown' && e.altKey) ||
          e.key === 'ArrowDown' ||
          e.key === 'ArrowUp'
        ) {
          return true
        }
        return false
      },
      keyMove: function (e) {
        if (e.ctrlKey || e.shiftKey) {
          return true
        }
        let p = true
        let $nw = $(e.target)
        switch (e.key) {
          case 'ArrowUp':
            $nw = this.listPrev($nw)
            break
          case 'ArrowDown':
            $nw = this.listNext($nw)
            break
          case 'Home':
            $nw = this.listFirst()
            break
          case 'End':
            $nw = this.listLast()
            break
          case 'Escape':
            $nw = this.listExit()
            break
          case 'ArrowLeft':
            $nw = this.listExit()
            break
          case 'ArrowRight':
            $nw = this.listExit()
            break
          default:
            p = false
        }
        if (p) {
          $nw.attr('tabindex', 0)
          $nw.focus()
          if (this.$sel) {
            this.$sel.attr('tabindex', -1)
          }
          this.$sel = $nw
          e.preventDefault()
          e.stopPropagation()
        }
      },
      listPrev: function ($nw) {
        if ($nw.parent().prev().length !== 0) {
          return $nw.parent().prev().children(':first-child')
        } else {
          return this.listLast()
        }
      },
      listNext: function ($nw) {
        if ($nw.parent().next().length !== 0) {
          return $nw.parent().next().children(':first-child')
        } else {
          return this.listFirst()
        }
      },
      listFirst: function () {
        return this.$list.children(':first-child').children(':first-child')
      },
      listLast: function () {
        return this.$list.children(':last-child').children(':first-child')
      },
      listExit: function () {
        this.mouseoutHandler()
        return this.$btn
      },
      listEnter: function () {
        return this.$list.children(':first-child').children(':first-child')
      }
    }
    return Popover
  })()

  $(document).ready(function () {
    const lng_popover = new Popover('version-popover')
  })
})()
