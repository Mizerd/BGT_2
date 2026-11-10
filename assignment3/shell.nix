let
  pkgs = import (builtins.fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/archive/70bdadeb94ffc8806c0570eb5c2695ad29f0e421.tar.gz";
    sha256 = "sha256-LWvKHp7kGxk/GEtlrGYV68qIvPHkU9iToomNFGagixU=";
  }) {};
in pkgs.mkShell {
  packages = with pkgs; [ gcc cmake gnumake pkg-config libbitcoin boost175 ];
}
