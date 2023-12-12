{ lib
, libX11
, libXres
, pandoc
, pkg-config
, procps
, stdenv
, systemd
, systemdSupport ? lib.meta.availableOn stdenv.hostPlatform systemd
, tomlc99
}:
stdenv.mkDerivation rec {
  pname = "gulp";
  version = lib.removeSuffix "\n" (builtins.readFile ../version.txt);

  src = ../.;

  makeFlags = [ "VERSION=${version}" ];

  installFlags = [ "PREFIX=$(out)" ]
    ++ lib.optionals systemdSupport [
    "SYSTEMD=1"
    "SYSTEMD_PREFIX=$(out)/etc/systemd"
  ];

  nativeBuildInputs = [
    pkg-config
    pandoc
  ];

  buildInputs = [
    libX11
    libXres
    procps
    tomlc99
  ];
}
