#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt
import zmq

import threading


def plotter(port, constellation, print_mer=False):
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    socket.connect(f'tcp://127.0.0.1:{port}')
    socket.setsockopt(zmq.SUBSCRIBE, b'')
    n = 0
    while True:
        message = socket.recv()
        z = np.frombuffer(message, 'complex64')
        constellation.set_xdata(z.real)
        constellation.set_ydata(z.imag)
        if print_mer:
            z_hard = (np.sign(z.real) + 1j * np.sign(z.imag))/np.sqrt(2)
            noise = np.average(np.abs(z - z_hard)**2)
            mer = 10 * np.log10(noise)
            print(f'MER = {mer:.1f} dB (packet {n})')
            n += 1


def main():
    plt.ion()
    fig = plt.figure()
    ax_header = fig.add_subplot(121)
    ax_payload = fig.add_subplot(122)
    axs = [ax_header, ax_payload]
    constellation_header = ax_header.plot([], [], '.')[0]
    constellation_payload = ax_payload.plot([], [], '.')[0]
    for ax in axs:
        a = np.sqrt(2) / 2
        ax.plot([a, a, -a, -a], [a, -a, a, -a], '.', color='red')
        ax.axis('equal')
        ax.set_xlim((-3, 3))
        ax.set_ylim((-3, 3))
    ax_header.set_title('Header')
    ax_payload.set_title('Payload')
    threading.Thread(target=plotter,
                     args=(5000, constellation_header)).start()
    threading.Thread(target=plotter,
                     args=(5001, constellation_payload, True)).start()
    plt.show(block=True)


if __name__ == '__main__':
    main()
