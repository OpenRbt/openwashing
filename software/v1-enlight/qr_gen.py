from qrcode import QRCode
from argparse import ArgumentParser

def qr_gen(data: str='', file: str=''):
    qr = QRCode(border=1)
    qr.add_data(data if data else 'no data')
    qr.make()
    qr.make_image(fill_color=(0, 0, 0), back_color=(255, 255, 255)).save(file if file else './qr.png')


if __name__ == '__main__':
    parser = ArgumentParser()
    parser.add_argument('--file', default='')
    parser.add_argument('--data', default='')

    args = parser.parse_args()

    qr_gen(args.data, file=args.file)
