"""Apply after the platform builder, which resets MKFSTOOL during setup."""
Import('env')
env.Replace(MKFSTOOL=env['K718_REPRO_FS_TOOL'])
