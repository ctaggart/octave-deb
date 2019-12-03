
GNU Octave is [distrubuted on Linux](https://wiki.octave.org/Octave_for_GNU/Linux) several ways. I want an easy way to install it on a GitHub Actions [virtual environment](https://help.github.com/en/actions/automating-your-workflow-with-github-actions/virtual-environments-for-github-hosted-runners). They support Ubuntu 18.04 Bionic, so that is the target here.

Debian Sid has a deb package avaiable for Octave 5.1.0, but it isn't available yet for Ubuntu. The latest for Ubuntu is Octave 4.4.1. The Docker image for Octave has Octave 5.1.0 on it. It's [Dockerfile](https://gitlab.com/mtmiller/docker-octave/blob/master/Dockerfile) downloads some binaries for Ubuntu and adds all the dependencies to the Docker image. For this deb package, the same binaries are used with the minimal set of dependencies to run `octave-cli`, not the GUI.

### Building
[cargo-deb](https://github.com/mmstick/cargo-deb) is used to create the Debian package. Install Rust with Cargo, then run:
``` sh
cargo install deb
```

Build the deb by running:
``` sh
cargo deb
```

You can then inspect the created package using:
``` sh
dpkg-deb --info target/debian/octave*.deb
dpkg-deb --contents target/debian/octave*.deb
```

### License

These builds scripts are under the MIT License. The GNU Octave software is under the [GNU GPL v3 License](https://gitlab.com/mtmiller/octave/blob/master/COPYING).