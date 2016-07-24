use "path:/usr/local/opt/libressl/lib" if osx
use "lib:ssl" if not windows
use "lib:crypto" if not windows
use "lib:libssl-32" if windows
use "lib:libcrypto-32" if windows

primitive _SSLInit
  """
  This initialises SSL when the program begins.
  """
  fun _init() =>
    @SSL_load_error_strings[None]()
    @SSL_library_init[I32]()
    let cb = @ponyint_ssl_multithreading[Pointer[U8]](@CRYPTO_num_locks[I32]())
    @CRYPTO_set_locking_callback[None](cb)
