
project('evcape', 'c')

libevdev = dependency('libevdev')
libudev = dependency('libudev')

executable('evcape',
  'evcape.c',
  c_args: [ '-Wno-unused-function' ],
  dependencies: [ libevdev, libudev ],
  install: true
)

