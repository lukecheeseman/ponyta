class List[A] is Seq[A]
  """
  A doubly linked list.
  """
  var _head: (ListNode[A] | None) = None
  var _tail: (ListNode[A] | None) = None
  var _size: USize = 0

  new create(len: USize = 0) =>
    """
    Do nothing, but be compatible with Seq.
    """
    None

  fun ref reserve(len: USize): List[A]^ =>
    """
    Do nothing, but be compatible with Seq.
    """
    this

  fun size(): USize =>
    """
    Returns the number of items in the list.
    """
    _size

  fun apply(i: USize = 0): this->A ? =>
    """
    Get the i-th element, raising an error if the index is out of bounds.
    """
    index(i)()

  fun ref update(i: USize, value: A): A^ ? =>
    """
    Change the i-th element, raising an error if the index is out of bounds.
    Returns the previous value, which may be None if the node has been popped
    but left on the list.
    """
    index(i)() = consume value

  fun index(i: USize): this->ListNode[A] ? =>
    """
    Gets the i-th node, raising an error if the index is out of bounds.
    """
    if i >= _size then
      error
    end

    var node = _head as this->ListNode[A]
    var j = USize(0)

    while j < i do
      node = node.next() as this->ListNode[A]
      j = j + 1
    end

    node

  fun ref remove(i: USize): List[A]^ ? =>
    """
    Remove the i-th node, raising an error if the index is out of bounds.
    """
    index(i).remove()
    this

  fun ref clear(): List[A]^ =>
    """
    Empties the list.
    """
    _head = None
    _tail = None
    _size = 0
    this

  fun head(): this->ListNode[A] ? =>
    """
    Get the head of the list.
    """
    _head as this->ListNode[A]

  fun tail(): this->ListNode[A] ? =>
    """
    Get the tail of the list.
    """
    _tail as this->ListNode[A]

  fun ref prepend_node(node: ListNode[A]): List[A]^ =>
    """
    Adds a node to the head of the list.
    """
    match _head
    | let head': ListNode[A] =>
      head'.prepend(node)
    else
      _set_both(node)
    end
    this

  fun ref append_node(node: ListNode[A]): List[A]^ =>
    """
    Adds a node to the tail of the list.
    """
    match _tail
    | let tail': ListNode[A] =>
      tail'.append(node)
    else
      _set_both(node)
    end
    this

  fun ref append_list(that: List[A]): List[A]^ =>
    """
    Remove all nodes from that and append them to this.
    """
    if this isnt that then
      while that._size > 0 do
        try append_node(that.head()) end
      end
    end
    this

  fun ref prepend_list(that: List[A]): List[A]^ =>
    """
    Remove all nodes from that and prepend them to this.
    """
    if this isnt that then
      while that._size > 0 do
        try prepend_node(that.tail()) end
      end
    end
    this

  fun ref push(a: A): List[A]^ =>
    """
    Adds a value to the tail of the list.
    """
    append_node(ListNode[A](consume a))

  fun ref pop(): A^ ? =>
    """
    Removes a value from the tail of the list.
    """
    tail().remove().pop()

  fun ref unshift(a: A): List[A]^ =>
    """
    Adds a value to the head of the list.
    """
    prepend_node(ListNode[A](consume a))

  fun ref shift(): A^ ? =>
    """
    Removes a value from the head of the list.
    """
    head().remove().pop()

  fun ref append(seq: ReadSeq[A], offset: USize = 0, len: USize = -1): List[A]^
  =>
    """
    Append the elements from a sequence, starting from the given offset.
    """
    if offset >= seq.size() then
      return this
    end

    let copy_len = len.min(seq.size() - offset)
    reserve(_size + copy_len)

    let cap = copy_len + offset
    var i = offset

    try
      while i < cap do
        push(seq(i))
        i = i + 1
      end
    end

    this

  fun ref truncate(len: USize): List[A]^ =>
    """
    Truncate the list to the given length, discarding excess elements.
    If the list is already smaller than len, do nothing.
    """
    try
      while _size > len do
        pop()
      end
    end

    this

  fun clone(): List[this->A!]^ =>
    """
    Clone the list.
    """
    let out = List[this->A!]

    for v in values() do
      out.push(v)
    end

    out

  fun nodes(): ListNodes[A, this->ListNode[A]]^ =>
    """
    Return an iterator on the nodes in the list.
    """
    ListNodes[A, this->ListNode[A]](_head)

  fun rnodes(): ListNodes[A, this->ListNode[A]]^ =>
    """
    Return an iterator on the nodes in the list.
    """
    ListNodes[A, this->ListNode[A]](_head, true)

  fun values(): ListValues[A, this->ListNode[A]]^ =>
    """
    Return an iterator on the values in the list.
    """
    ListValues[A, this->ListNode[A]](_head)

  fun rvalues(): ListValues[A, this->ListNode[A]]^ =>
    """
    Return an iterator on the values in the list.
    """
    ListValues[A, this->ListNode[A]](_head, true)

  fun ref _increment() =>
    _size = _size + 1

  fun ref _decrement() =>
    _size = _size - 1

  fun ref _set_head(head': (ListNode[A] | None)) =>
    _head = head'

  fun ref _set_tail(tail': (ListNode[A] | None)) =>
    _tail = tail'

  fun ref _set_both(node: ListNode[A]) =>
    node._set_list(this)
    _head = node
    _tail = node
    _size = 1

class ListNodes[A, N: ListNode[A] #read] is Iterator[N]
  """
  Iterate over the nodes in a list.
  """
  var _next: (N | None)
  let _reverse: Bool

  new create(head: (N | None), reverse: Bool = false) =>
    """
    Keep the next list node to be examined.
    """
    _next = head
    _reverse = reverse

  fun has_next(): Bool =>
    """
    If we have a list node, we have more values.
    """
    _next isnt None

  fun ref next(): N ? =>
    """
    Get the list node and replace it with the next one.
    """
    match _next
    | let next': N =>
      if _reverse then
        _next = next'.prev()
      else
        _next = next'.next()
      end

      next'
    else
      error
    end

class ListValues[A, N: ListNode[A] #read] is Iterator[N->A]
  """
  Iterate over the values in a list.
  """
  var _next: (N | None)
  let _reverse: Bool

  new create(head: (N | None), reverse: Bool = false) =>
    """
    Keep the next list node to be examined.
    """
    _next = head
    _reverse = reverse

  fun has_next(): Bool =>
    """
    If we have a list node, we have more values.
    """
    _next isnt None

  fun ref next(): N->A ? =>
    """
    Get the value of the list node and replace it with the next one.
    """
    match _next
    | let next': N =>
      if _reverse then
        _next = next'.prev()
      else
        _next = next'.next()
      end

      next'()
    else
      error
    end
