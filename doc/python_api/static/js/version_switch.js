(function() {  // switch: v1.4
"use strict";

var versionsFileUrl = "https://docs.blender.org/PROD/versions.json"

var all_versions;

class Popover {
  constructor(id)
  {
    this.isOpen = false;
    this.type = (id === "version-popover");
    this.btn = document.querySelector('#' + id);
    this.dialog = this.btn.nextElementSibling;
    this.list = this.dialog.querySelector("ul");
    this.sel = null;
    const that = this;
    this.btnClickHandler = function(e) {
      that.init();
      e.preventDefault();
      e.stopPropagation();
    };
    this.btnKeyHandler = function(e) {
      if (that.btnKeyFilter(e)) {
        that.init();
        e.preventDefault();
        e.stopPropagation();
      }
    };
    this.btn.addEventListener("click", this.btnClickHandler);
    this.btn.addEventListener("keydown", this.btnKeyHandler);
  }

  init()
  {
    this.btn.removeEventListener("click", this.btnClickHandler);
    this.btn.removeEventListener("keydown", this.btnKeyHandler);

    new Promise((resolve, reject) => {
      if (all_versions === undefined) {
        this.btn.classList.add("wait");
        fetch(versionsFileUrl)
            .then((response) => response.json())
            .then((data) => {
              all_versions = data;
              resolve();
            })
            .catch(() => {
              console.error("Version Switch Error: versions.json could not be loaded.");
              this.btn.classList.remove("disabled");
            });
      }
      else {
        resolve();
      }
    }).then(() => {
      let release = DOCUMENTATION_OPTIONS.VERSION;
      const m = release.match(/\d\.\d+/g);
      if (m) {
        release = m[0];
      }

      this.warnOld(release, all_versions);

      const version = this.getNamed(release);
      this.buildList(version);

      this.list.firstElementChild.remove();
      const that = this;
      this.list.addEventListener("keydown", function(e) {
        that.keyMove(e);
      });

      this.btn.classList.remove("wait");
      this.btnOpenHandler();
      this.btn.addEventListener("mousedown", function(e) {
        that.btnOpenHandler();
        e.preventDefault()
      });
      this.btn.addEventListener("keydown", function(e) {
        if (that.btnKeyFilter(e)) {
          that.btnOpenHandler();
        }
      });
    });
  }
  warnOld(release, all_versions)
  {
    // Note this is effectively disabled now, two issues must fixed:
    // * versions.js does not contain a current entry, because that leads to
    //   duplicate version numbers in the menu. These need to be deduplicated.
    // * It only shows the warning after opening the menu to switch version
    //   when versions.js is loaded. This is too late to be useful.
    let current = all_versions.current
    if (!current) {
      // console.log("Version Switch Error: no 'current' in version.json.");
      return;
    }
    const m = current.match(/\d\.\d+/g);
    if (m) {
      current = parseFloat(m[0]);
    }
    if (release < current) {
      const currentURL = window.location.pathname.replace(release, current);
      const warning =
          document.querySelector("template#version-warning").firstElementChild.cloneNode(true);
      const link = warning.querySelector('a');
      link.setAttribute('href', currentURL);
      link.textContent = current;

      let body = document.querySelector("div.body");
      if (!body.length) {
        body = document.querySelector("div.document");
      }
      body.prepend(warning);
    }
  }
  buildList(v)
  {
    const url = new URL(window.location.href);
    let pathSplit = [ "", "api", v ];
    if (url.pathname.startsWith("/api/")) {
      pathSplit.push(url.pathname.split('/').slice(4).join('/'));
    }
    else {
      pathSplit.push(url.pathname.substring(1));
    }
    let dyn, cur;
    if (this.type) {
      dyn = all_versions;
      cur = v;
    }
    const that = this;
    const template = document.querySelector("template#version-entry").content;
    for (let [ix, title] of Object.entries(dyn)) {
      let clone;
      if (ix === cur) {
        clone = template.querySelector("li.selected").cloneNode(true);
        clone.querySelector("span").innerHTML = title;
      }
      else {
        pathSplit[1 + that.type] = ix;
        let href = new URL(url);
        href.pathname = pathSplit.join('/');
        clone = template.firstElementChild.cloneNode(true);
        const link = clone.querySelector("a");
        link.href = href;
        link.innerHTML = title;
      }
      that.list.append(clone);
    };
    return this.list;
  }
  getNamed(v)
  {
    for (let [ix, title] of Object.entries(all_versions)) {
      if (ix === "master" || ix === "main" || ix === "latest") {
        const m = title.match(/\d\.\d[\w\d\.]*/)[0];
        if (parseFloat(m) == v) {
          v = ix;
          return false;
        }
      }
    };
    return v;
  }
  dialogToggle(speed)
  {
    const wasClose = !this.isOpen;
    const that = this;
    if (!this.isOpen) {
      this.btn.classList.add("version-btn-open");
      this.btn.setAttribute("aria-pressed", true);
      this.dialog.setAttribute("aria-hidden", false);
      this.dialog.style.display = "block";
      this.dialog.animate({opacity : [ 0, 1 ], easing : [ 'ease-in', 'ease-out' ]}, speed)
          .finished.then(() => {
            this.focusoutHandlerPrime = function(e) {
              that.focusoutHandler();
              e.stopImmediatePropagation();
            };
            this.mouseoutHandlerPrime = function(e) {
              that.mouseoutHandler();
              e.stopImmediatePropagation();
            };
            this.btn.parentNode.addEventListener("focusout", this.focusoutHandlerPrime);
            this.btn.parentNode.addEventListener("mouseleave", this.mouseoutHandlerPrime);
          });
      this.isOpen = true;
    }
    else {
      this.btn.classList.remove("version-btn-open");
      this.btn.setAttribute("aria-pressed", false);
      this.dialog.setAttribute("aria-hidden", true);
      this.btn.parentNode.removeEventListener("focusout", this.focusoutHandlerPrime);
      this.btn.parentNode.removeEventListener("mouseleave", this.mouseoutHandlerPrime);
      this.dialog.animate({opacity : [ 1, 0 ], easing : [ 'ease-in', 'ease-out' ]}, speed)
          .finished.then(() => {
            this.dialog.style.display = "none";
            if (this.sel) {
              this.sel.setAttribute("tabindex", -1);
            }
            this.btn.setAttribute("tabindex", 0);
            if (document.activeElement !== null && document.activeElement !== document &&
                document.activeElement !== document.body)
            {
              this.btn.focus();
            }
          });
      this.isOpen = false;
    }

    if (wasClose) {
      if (this.sel) {
        this.sel.setAttribute("tabindex", -1);
      }
      if (document.activeElement !== null && document.activeElement !== document &&
          document.activeElement !== document.body)
      {
        const nw = this.listEnter();
        nw.setAttribute("tabindex", 0);
        nw.focus();
        this.sel = nw;
      }
    }
  }
  btnOpenHandler()
  {
    this.dialogToggle(300);
  }
  focusoutHandler()
  {
    const list = this.list;
    const that = this;
    setTimeout(function() {
      if (!list.querySelector(":focus")) {
        that.dialogToggle(200);
      }
    }, 200);
  }
  mouseoutHandler()
  {
    this.dialogToggle(200);
  }
  btnKeyFilter(e)
  {
    if (e.ctrlKey || e.shiftKey) {
      return false;
    }
    if (e.key === " " || e.key === "Enter" || (e.key === "ArrowDown" && e.altKey) ||
        e.key === "ArrowDown" || e.key === "ArrowUp")
    {
      return true;
    }
    return false;
  }
  keyMove(e)
  {
    if (e.ctrlKey || e.shiftKey) {
      return true;
    }
    let nw = e.target;
    switch (e.key) {
      case "ArrowUp":
        nw = this.listPrev(nw);
        break;
      case "ArrowDown":
        nw = this.listNext(nw);
        break;
      case "Home":
        nw = this.listFirst();
        break;
      case "End":
        nw = this.listLast();
        break;
      case "Escape":
        nw = this.listExit();
        break;
      case "ArrowLeft":
        nw = this.listExit();
        break;
      case "ArrowRight":
        nw = this.listExit();
        break;
      default:
        return false;
    }
    nw.setAttribute("tabindex", 0);
    nw.focus();
    if (this.sel) {
      this.sel.setAttribute("tabindex", -1);
    }
    this.sel = nw;
    e.preventDefault();
    e.stopPropagation();
  }
  listPrev(nw)
  {
    if (nw.parentNode.previousElementSibling.length !== 0) {
      return nw.parentNode.previousElementSibling.firstElementChild;
    }
    else {
      return this.listLast();
    }
  }
  listNext(nw)
  {
    if (nw.parentNode.nextElementSibling.length !== 0) {
      return nw.parentNode.nextElementSibling.firstElementChild;
    }
    else {
      return this.listFirst();
    }
  }
  listFirst()
  {
    return this.list.firstElementChild.firstElementChild;
  }
  listLast()
  {
    return this.list.lastElementChild.firstElementChild;
  }
  listExit()
  {
    this.mouseoutHandler();
    return this.btn;
  }
  listEnter()
  {
    return this.list.firstElementChild.firstElementChild;
  }
}

document.addEventListener('DOMContentLoaded', () => { new Popover("version-popover"); });
})();
