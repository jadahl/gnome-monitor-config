gnome = import('gnome')

cairo = dependency('cairo')
glib = dependency('glib-2.0')
gio = dependency('gio-2.0')
gio_unix = dependency('gio-unix-2.0')

src = [ 'gmc-display-config.c',
    'gmc-display-config.h',
    'gmc-display-config-manager.c',
    'gmc-display-config-manager.h',
    'gmc-display-test.c'
]

gen_src = gnome.gdbus_codegen('gmc-dbus-display-config',
                              'org.gnome.Mutter.DisplayConfig.xml',
                              interface_prefix: 'org.gnome.Mutter.',
                              namespace: 'CcDbus')
src += gen_src

executable('gnome-monitor-config', src, install : true,
           dependencies : [ glib, gio, gio_unix, cairo ])
