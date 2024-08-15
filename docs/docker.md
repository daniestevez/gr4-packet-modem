# Docker image usage

## gr4-packet-modem

The `gr4-packet-modem` docker image can be run as follows.

```
docker run --rm --net host -e TERM -e HOME=/home/ubuntu \
    --name=gr4 --hostname=gr4 \
    --cap-add=NET_ADMIN --cap-add=SYS_ADMIN \
    --privileged -v /dev/bus/usb:/dev/bus/usb \
    --device /dev/net/tun \
    -v /var/run/netns:/var/run/netns \
    -v $HOME/gr4-packet-modem:/gr4-packet-modem \
    --user ubuntu \
    --entrypoint bash \
    -it ghcr.io/daniestevez/gr4-packet-modem
```

This assumes that `$HOME/gr4-packet-modem` contains a checkout of the
gr4-packet-modem repository, which is made available to the docker container at
`/gr4-packet-modem`. It also assumes that `$UID` is 1000. This is the UID of the
user `ubuntu` in the container, so if there is a UID mismatch there will be file
permission issues when sharing the `gr4-packet-modem` checkout between the
container and the host.

Several of the options used to run the container are required for TUN device,
network namespace, and SDR access: `--cap-add=NET_ADMIN` and
`--device /dev/net/tun` are needed to access TUN devices; `--cap-add=SYS_ADMIN`
and `-v /var/run/netns:/var/run/netns` are needed for network namespace access;
`--privileged` and `-v /dev/bus/usb:/dev/bus/usb` are needed to access USB SDRs.

It can be useful to mount a volume in the container `$HOME` directory, so as to
preserve bash history, etc. This can be done by adding

```
-v gr4_home:/home/ubuntu
```

to the above `docker run` command.

The applications in the `apps` directory need to be run as root, because they
use network namespaces. While the docker container is running, run

```
docker exec -u0 -it gr4 bash
```

to get a root shell in the container from which to run these applications.

## gr4-packet-modem-built

The `gr4-packet-modem-built` docker image can be run as follows.

```
docker run --rm --net host -e TERM \
    --name=gr4-built --hostname=gr4-built \
    --cap-add=NET_ADMIN --cap-add=SYS_ADMIN \
    --privileged -v /dev/bus/usb:/dev/bus/usb \
    --device /dev/net/tun \
    -v /var/run/netns:/var/run/netns \
    -v $HOME/gr4-packet-modem:/gr4-packet-modem \
    -it ghcr.io/daniestevez/gr4-packet-modem-built
```

In this case, the `$HOME/gr4-packet-modem` checkout is also shared with the
container so as to have a shared directory in which to place FIFO files (with
`mkfifo`). These are used to connect a GNU Radio 4.0 application running in the
container with a GNU Radio 3.10 flowgraph running on the host.

As in the case of the `gr4-packet-modem` image, the applications in `apps` need
to be run as root. A root shell can be obtained with

```
docker exec -u0 -it gr4-built bash
```
