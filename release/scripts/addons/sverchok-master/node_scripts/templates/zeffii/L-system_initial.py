# https://groups.google.com/d/msg/tributary/9kI-CGVVQb4/J6joc8q6VhwJ

import math
from math import sin, cos, radians, degrees, pi, pow

import random
from random import random

import ast
from ast import literal_eval


def sv_main(i_user=8, bend=-10.71875, arm_dist=5890):

    def to_rad(degrees):
        return degrees * 2 * pi / 360

    in_sockets = [
        ['s', 'i_user', i_user],
        ['s', 'bend', bend],
        ['s', 'arm_dist', arm_dist]]

    verts_out = []
    out_sockets = [
        ['v', 'verts', verts_out]
    ]

    bend = 2 * bend                       # angle to twist (degrees)
    iterations = min(8, i_user)           # number of rewrites (recursion levels in grammar)
    F = arm_dist / (pow(2.4, iterations)) # arm distance
    x_start = 0
    y_start = 0

    Lvals = {
        'angle_offset': to_rad(bend),     # angle as radian
        'angle_rad': 0,                   # the current angle for drawing
        'x_start': x_start,               # drawing start x
        'y_start': y_start,               # drawing start y
        'x_current': x_start,             # current cursor position x
        'y_current': y_start              # current cursor position y
    }
    rules = {}                            # a map of the rules
    commands = {}                         # a map of the commands

    def get_xy(angle_rad, Lv):
        return {'x': cos(angle_rad) * Lv, 'y': sin(angle_rad) * Lv}

    def command_from_rad(command):
        command_parts = command.split(" ")
        Lv = literal_eval(command_parts[1])
        if len(command_parts) > 2:
            if command_parts[2] == "R":
                Lv = Lv * (1 + (literal_eval(command_parts[3]) * (random() - 0.5)) / 100)

        c = get_xy(Lvals['angle_rad'], Lv)
        Lvals['x_current'] += c['x']
        Lvals['y_current'] += c['y']

        if (command_parts[0] == "L"):
            # tributary.ctx.lineTo(x_current, y_current)
            verts_out.append((Lvals['x_current'], Lvals['y_current'], 0))

        if (command_parts[0] == "M"):
            # tributary.ctx.stroke()
            # tributary.ctx.moveTo(x_current, y_current)
            verts_out.append((Lvals['x_current'], Lvals['y_current'], 0))

    def make_path(rules, commands, start_string):
        # tributary.ctx.moveTo(x_start,y_start)

        verts_out.append((0, 0, 0))

        recursive_path(rules, commands, start_string, 0)
        # tributary.ctx.stroke()

    # L-systems grammar rewriting
    def recursive_path(rules, commands, input_string, iteration):
        token = ""

        for i in range(len(input_string)):
            token = input_string[i]
            if (token == '-'):
                Lvals['angle_rad'] += Lvals['angle_offset']
            elif (token == '+'):
                Lvals['angle_rad'] -= Lvals['angle_offset']
            elif (token in rules):

                if (iteration >= iterations and token in commands):
                    ''' if we're at the bottom level, move forward '''
                    command_from_rad(commands[token])

                else:
                    ''' else go deeper down the rabbit hole... '''
                    recursive_path(rules, commands, rules[token], iteration + 1)

            elif (token in commands):
                command_from_rad(commands[token])

    '''
    The rules:
    + means turn left by bend degrees
    - means turn right by bend degrees
    Variables (like A or B) get replaced by the rule when encountered
    This keeps repeating until the maximum number of iterations is reached
    You need one rule entry per variable used
    '''

    rules["A"] = "BB----BB+++++++++++++BB----BB"
    rules["B"] = "-----------A++++++++++++++"

    '''
    The commands:
    "L nn" means draw a line of length nn, e.g. L90 means draw a line 90 pixels long
    "M nn" means move nn pixels forward, e.g. M90 means move the current cursor 90 pixels forward
    "R nn" at the end of the command means add +/- nn% random noise to the length
    '''
    commands["A"] = "L " + str(F)
    start = "A"

    make_path(rules, commands, start)

    out_sockets[0][2] = [verts_out]
    return in_sockets, out_sockets
