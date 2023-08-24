
project('evcape', 'c')

libevdev = dependency('libevdev')
libudev = dependency('libudev')

executable('evcape',
  'evcape.c',
  dependencies: [ libevdev, libudev ],
)

