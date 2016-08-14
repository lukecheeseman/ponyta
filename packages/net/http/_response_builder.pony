use "buffered"
use "net"

class _ResponseBuilder is TCPConnectionNotify
  """
  This builds a response payload using received chunks of data.
  """
  let _client: _ClientConnection
  let _buffer: Reader = Reader
  let _builder: _PayloadBuilder = _PayloadBuilder.response()

  new iso create(client: _ClientConnection) =>
    """
    The response builder needs to know which client to forward the response to.
    """
    _client = client

  fun ref connected(conn: TCPConnection ref) =>
    """
    Tell the client we have connected.
    """
    _client._connected(conn)

  fun ref connect_failed(conn: TCPConnection ref) =>
    """
    The connection could not be established. Tell the client not to proceed.
    """
    _client._connect_failed(conn)

  fun ref auth_failed(conn: TCPConnection ref) =>
    """
    SSL authentication failed. Tell the client not to proceed.
    """
    _client._auth_failed(conn)

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    """
    Assemble chunks of data into a response. When we have a whole response,
    give it to the client and start a new one.
    """
    // TODO: inactivity timer
    _buffer.append(consume data)
    _dispatch(conn)

  fun ref closed(conn: TCPConnection ref) =>
    """
    The connection has closed, possibly prematurely.
    """
    _builder.closed(_buffer)
    _buffer.clear()
    _dispatch(conn)
    _client._closed(conn)

  fun ref _dispatch(conn: TCPConnection ref) =>
    """
    Dispatch responses if we have any.
    """
    while true do
      _builder.parse(_buffer)

      match _builder.state()
      | _PayloadReady =>
        _client._response(_builder.done())
      | _PayloadError =>
        _client._closed(conn)
        break
      else
        break
      end
    end
