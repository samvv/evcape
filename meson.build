
project('evcape', 'c')

libevdev = dependency('libevdev')
libudev = dependency('libudev')

executable('evcape',
  'evcape.c',
  'evcape_opts.c',
  dependencies: [ libevdev, libudev ],
)

