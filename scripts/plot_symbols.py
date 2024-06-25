#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt
import zmq

import threading


def plotter(port, constellation):
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    socket.connect(f'tcp://127.0.0.1:{port}')
    socket.setsockopt(zmq.SUBSCRIBE, b'')
    while True:
        message = socket.recv()
        z = np.frombuffer(message, 'complex64')
        constellation.set_xdata(z.real)
        constellation.set_ydata(z.imag)


def main():
    plt.ion()
    fig = plt.figure()
    ax_header = fig.add_subplot(121)
    ax_payload = fig.add_subplot(122)
    constellation_header = ax_header.plot([], [], '.')[0]
    constellation_payload = ax_payload.plot([], [], '.')[0]
    for ax in [ax_header, ax_payload]:
        ax.set_xlim((-2, 2))
        ax.set_ylim((-2, 2))
        ax.axis('equal')
    ax_header.set_title('Header')
    ax_payload.set_title('Payload')
    threading.Thread(target=plotter,
                     args=(5000, constellation_header)).start()
    threading.Thread(target=plotter,
                     args=(5001, constellation_payload)).start()
    plt.show(block=True)


if __name__ == '__main__':
    main()
