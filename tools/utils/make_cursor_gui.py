#!/usr/bin/python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Created by Robert Wenzlaff (Det. Thorn).
# Oct. 30, 2003

from tkinter import (
    Button,
    Canvas,
    Checkbutton,
    END,
    Frame,
    IntVar,
    Label,
    RIDGE,
    Text,
    Tk,
)

color = ("black", "white", "darkgreen", "gray")


class App:

    def __init__(self, master):
        frame = Frame(master, borderwidth=5)
        frame.grid(column=0, row=0, pady=5)

        self.state = []
        self.states = 256
        self.laststate = 2  # 0=Black, 1=White, 2=Transparent.

        self.size = 16
        self.gridsz = 20

        for x in range(1024):
            self.state.append(2)

        self.screen = Canvas(frame, height=320, width=320, bg=color[2])
        self.screen.bind("<Button-1>", self.scrnclick1)
        self.screen.bind("<Button-3>", self.scrnclick2)
        self.screen.bind("<B1-Motion>", self.scrndrag)

        for x in range(16):
            self.screen.create_line((x * 20, 0, x * 20, 320), fill=color[3])
            self.screen.create_line((0, x * 20, 320, x * 20), fill=color[3])

        self.screen.grid(row=0, column=0, columnspan=5)

        frame2 = Frame(master, borderwidth=5)
        frame2.grid(column=0, row=1, pady=5)

        self.clear = Button(frame2, text="Clear", command=self.clearit)
        self.clear.grid(row=0, column=0, pady=20)

        self.doit = Button(frame2, text="Print", command=self.doit)
        self.doit.grid(row=0, column=1, pady=20)

        # self.doitlab = Label(frame2, text="(Output to stdout)");
        # self.doitlab.grid(row=1, column=1);

        self.parse = Button(frame2, text="Parse", command=self.parsetext)
        self.parse.grid(row=0, column=2, pady=20)

        self.large = 0
        self.dummy = IntVar()
        self.largeb = Checkbutton(frame2, text="Large", var=self.dummy, command=self.changesize)
        self.largeb.grid(row=0, column=3, pady=20)

        self.prev = Canvas(frame2, height=17, width=17, bg=color[2], relief=RIDGE)
        self.prev.grid(row=0, column=4, pady=20, padx=20)

        # DataParsers
        self.bmlabel = Label(frame2, text="Bitmap Data (paste hex from code)")
        self.bmlabel.grid(row=2, column=0, columnspan=5, sticky="W")

        self.bmentry = Text(frame2, width=80, height=9, font="Times 8")
        self.bmentry.bind("<Leave>", self.bmtextpaste)
        self.bmentry.grid(row=3, column=0, columnspan=5, pady=5)

        self.msklabel = Label(frame2, text="Mask Data (paste hex from code)")
        self.msklabel.grid(row=4, column=0, columnspan=5, sticky="W")

        self.mskentry = Text(frame2, width=80, height=9, font="Times 8")
        self.mskentry.bind("<Leave>", self.msktextpaste)
        self.mskentry.grid(row=5, column=0, columnspan=5, pady=5)

    def changesize(self):
        self.large = ~self.large
        if self.large:
            self.size = 32
            self.gridsz = 10
            self.states = 1024
            oldstate = self.state
            self.state = []
            for n in range(1024):
                col = (n // 2) % 16
                row = int(n // 64)
                self.state.append(oldstate[16 * row + col])
            oldstate = []
        else:
            self.size = 16
            self.gridsz = 20
            self.states = 256
            oldstate = self.state
            self.state = []
            for n in range(1024):
                if not ((n % 2) or ((n // 32) % 2)):
                    self.state.append(oldstate[n])
            for n in range(256, 1024):
                self.state.append(2)
            oldstate = []

        # Insert scaling here

        self.updatescrn()
        self.prev.config(width=self.size + 1, height=self.size + 1)
        for n in range(self.states):
            self.updateprev(n)
        # self.prev.grid(row=0, column=4, padx=self.gridsz, pady=self.gridsz)

    def scrnclick1(self, event):
        self.scrnclick(event, 1)

    def scrnclick2(self, event):
        self.scrnclick(event, -1)

    def scrnclick(self, event, direction):
        if (event.x > 319) or (event.y > 319) or (event.x < 0) or (event.y < 0):
            return

        n = (event.x // self.gridsz) + self.size * (event.y // self.gridsz)

        self.state[n] += direction
        self.state[n] %= 3

        row = n % self.size
        col = n // self.size

        self.screen.create_rectangle((self.gridsz * row + 1,
                                      self.gridsz * col + 1,
                                      self.gridsz * row + self.gridsz - 1,
                                      self.gridsz * col + self.gridsz - 1),
                                     fill=color[self.state[n]], outline="")

        self.laststate = self.state[n]
        self.updateprev(n)

    def scrndrag(self, event):
        if (event.x > 319) or (event.y > 319) or (event.x < 0) or (event.y < 0):
            return

        n = (event.x // self.gridsz) + self.size * (event.y // self.gridsz)

        row = n % self.size
        col = n // self.size

        self.screen.create_rectangle((self.gridsz * row + 1,
                                      self.gridsz * col + 1,
                                      self.gridsz * row + self.gridsz - 1,
                                      self.gridsz * col + self.gridsz - 1),
                                     fill=color[self.laststate], outline="")
        self.state[n] = self.laststate

        self.updateprev(n)

    def updateprev(self, n):
        x = n % self.size + 1
        y = n // self.size + 1

        if self.large:
            pad = 12
        else:
            pad = 20

        self.prev.create_line(x + 1, y + 1, x + 2, y + 1, fill=color[self.state[n]])
        self.prev.grid(row=0, column=4, padx=pad, pady=pad)

    def updatescrn(self):
        self.screen.create_rectangle(0, 0, 320, 320, fill=color[2])
        for x in range(self.size):
            self.screen.create_line((x * self.gridsz, 0, x * self.gridsz, 320), fill=color[3])
            self.screen.create_line((0, x * self.gridsz, 320, x * self.gridsz), fill=color[3])
        for n in range(self.states):
            row = n % self.size
            col = n // self.size
            self.screen.create_rectangle((self.gridsz * row + 1,
                                          self.gridsz * col + 1,
                                          self.gridsz * row + self.gridsz - 1,
                                          self.gridsz * col + self.gridsz - 1),
                                         fill=color[self.state[n]], outline="")

    def bmtextpaste(self, event):
        string = self.bmentry.get(1.0, END)
        self.bmentry.delete(1.0, END)
        string = string.replace("\t", "")
        self.bmentry.insert(END, string)

    def msktextpaste(self, event):
        string = self.mskentry.get(1.0, END)
        self.mskentry.delete(1.0, END)
        string = string.replace("\t", "")
        self.mskentry.insert(END, string)

    def parsetext(self):
        bmstring = self.bmentry.get(1.0, END)
        bmstring = bmstring.replace(",", " ")
        bmstring = bmstring.split()

        mskstring = self.mskentry.get(1.0, END)
        mskstring = mskstring.replace(",", " ")
        mskstring = mskstring.split()

        if len(bmstring) != len(mskstring):
            print("Mismatched data. Bitmap and mask must be same size,")
            return
        elif not (len(bmstring) == 32 or len(bmstring) == 128):
            print("Size Error, input must be 32 or 128 hex bytes. ")
            return

        for n in range(self.states):
            self.state[n] = 0

        m = 0
        for entry in bmstring:
            e = int(entry, 16)
            for bit in range(8):
                self.state[m] = (e & 1)
                e = e >> 1
                m += 1

        m = 0
        for entry in mskstring:
            e = int(entry, 16)
            for bit in range(8):
                if not (e & 1):
                    self.state[m] = 2
                e = e >> 1
                m += 1

        self.updatescrn()
        for n in range(self.states):
            self.updateprev(n)

    def clearit(self):
        for n in range(self.states):
            self.state[n] = 2
            self.updateprev(n)
        self.updatescrn()
        self.bmentry.delete(0.0, END)
        self.mskentry.delete(0.0, END)

    def doit(self):
        mask = []
        bitmap = []
        numbytes = self.size * self.size // 8
        for i in range(numbytes):
            m = 0
            b = 0
            for j in range(8):
                m <<= 1
                b <<= 1
                if (self.state[(i * 8) + (7 - j)] != 2):
                    m |= 1
                if (self.state[(i * 8) + (7 - j)] == 1):
                    b |= 1
                # print((i * 8) + (7 - j), self.state[(i * 8) + (7 - j)], m)
            mask.append(m)
            bitmap.append(b)

        print("\n\nstatic char bitmap[] = {", end=' ')
        for i in range(numbytes):
            b1 = bitmap[i]
            if not (i % 8):
                print("\n\t", end=' ')
            print("0x%(b1)02x, " % vars(), end=' ')
        print("\n};")

        print("\nstatic char mask[] = {", end=' ')
        for i in range(numbytes):
            b1 = mask[i]
            if not (i % 8):
                print("\n\t", end=' ')
            print("0x%(b1)02x, " % vars(), end=' ')
        print("\n};")


################## Main App #######################
root = Tk()

app = App(root)
root.title("Cursor Maker")

root.mainloop()
