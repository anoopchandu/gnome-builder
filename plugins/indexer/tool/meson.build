sources = [
	'ide-ast-indexer.c',
	'ide-ast-indexer.h',
	'ide-simple-table-builder.c',
	'ide-simple-table-builder.h',
	'main.c',
]

llvm_config = find_program(
  'llvm-config',
  'llvm-config-4.0', 'llvm-config40',
  'llvm-config-3.9', 'llvm-config39',
  'llvm-config-3.8', 'llvm-config38',
  'llvm-config-3.7', 'llvm-config37',
  'llvm-config-3.6', 'llvm-config36',
  'llvm-config-3.5', 'llvm-config35',
  'llvm-config-5.0', 'llvm-config-devel',
)

ret = run_command(llvm_config, '--includedir')
if ret.returncode() != 0
  error('clang: Failed to run ' + llvm_config.path())
endif
clang_include = ret.stdout().strip()

ret = run_command(llvm_config, '--libdir')
if ret.returncode() != 0
  error('clang: Failed to run ' + llvm_config.path())
endif
clang_libdir = ret.stdout().strip().split(' ')

if not cc.has_header('clang-c/Index.h', args: '-I' + clang_include)
  error('clang: Failed to find headers')
endif

clang_dep = declare_dependency(
  dependencies: cc.find_library('libclang', dirs: clang_libdir),
  include_directories: include_directories(clang_include),
)

executable ('indexer', sources,
	         dependencies : [libdazzle_dep, clang_dep],
	         install: true,)
