"""Create the documented TS3 ZIP-based plugin package; never bundle Qt runtimes."""
import argparse
import hashlib
import pathlib
import re
import shutil
import zipfile

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=pathlib.Path, required=True)
    parser.add_argument('--version', default='1.1.1')
    parser.add_argument('--output', type=pathlib.Path, default=pathlib.Path('dist'))
    args = parser.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:[-.][A-Za-z0-9]+)*', args.version):
        parser.error('Invalid version')
    platform = {'whostalkin_win64.dll': 'win64', 'whostalkin_linux_amd64.so': 'linux_amd64'}.get(args.binary.name)
    if not platform or not args.binary.is_file():
        parser.error('Expected built whostalkin_win64.dll or whostalkin_linux_amd64.so')
    args.output.mkdir(parents=True, exist_ok=True)
    artifact = args.output / args.binary.name
    if args.binary.resolve() != artifact.resolve():
        shutil.copy2(args.binary, artifact)
    package = args.output / f'WhosTalkin-{args.version}-{platform}.ts3_plugin'
    ini = (f'Name = WhosTalkin\nType = Plugin\nAuthor = Khivus\n'
           f'Version = {args.version}\nPlatforms = {platform}\n'
           'Description = "Active speakers and incoming whispers. TS3 3.6.x / API 26 / Qt 5.15.2."\n')
    with zipfile.ZipFile(package, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('package.ini', ini)
        archive.write(artifact, 'plugins/' + artifact.name)
        license_path = pathlib.Path(__file__).resolve().parents[1] / 'LICENSE'
        archive.write(license_path, 'plugins/whostalkin/LICENSE.txt')
    shutil.copy2(license_path, args.output / 'LICENSE.txt')
    with (args.output / 'SHA256SUMS.txt').open('w', encoding='utf-8', newline='\n') as sums:
        for path in (artifact, package):
            sums.write(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.name + '\n')
    print(package.resolve())

if __name__ == '__main__':
    main()
