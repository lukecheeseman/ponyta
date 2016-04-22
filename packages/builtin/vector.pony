class Vector[A, _size: USize]
  """
  Contiguous, fixed sized memory to store elements of type A.
  """

  new _create() =>
    """
    Create a vector of len elements, populating them with random memory. For
    internal use only.
    """
    true

  fun _apply(i: USize): this->A =>
    """
    Get the i-th element
    """
    compile_intrinsic

  fun ref _update(i: USize, value: A): A^ =>
    """
    Change the i-th element
    """
    compile_intrinsic

  new init(from: Iterator[A^]) ? =>
    """
    Create a vector, initialised from the given sequence.
    """
    //generate(lambda ref(i: USize)(from): A^ ? => from.next() end)
    var i: USize = 0
    while i < _size do
      _update(i, from.next())
      i = i + 1
    end

  new duplicate(from: A^) =>
    """
    Create a vector, initiliased to the given value
    """
    var i: USize = 0
    while i < _size do
      _update(i, from)
      i = i + 1
    end

  new generate(f: {ref(USize): A^ ?} ref) ? =>
    """
    Create a vector initiliased using a generator function
    """
    var i: USize = 0
    while i < _size do
      _update(i, f(i))
      i = i + 1
    end

  new undefined[B: (A & Real[B] val & Number) = A]() =>
    """
    Create a vector of len elements, populating them with random memory. This
    is only allowed for a vector of numbers.
    """
    true

  fun filter(f: {val(box->A): Bool} val): Array[this->A!] =>
    let elems = Array[this->A]
    var i: USize = 0
    while i < _size do
      let elem = _apply(i)
      if f(elem) then
        elems.push(elem)
      end
      i = i + 1
    end
    elems

  fun copy_to(dst: Vector[this->A!, _size]) =>
    var i: USize = 0
    while i < _size do
      dst._update(i, _apply(i))
      i = i + 1
    end
    //let me: this->Vector[A, _size] = this
    //dst.generate(lambda ref(i: USize)(me): A ? => me(i) end)

  fun string(f: {val(box->A!): String} val): String ref^ =>
    let array = Array[String]
    for value in values() do
      array.push(f(value))
    end
    String.append("[").append(", ".join(array)).append("]")

  fun size(): USize =>
    """
    Return the parameterised size, this lets us treat the Vector as a ReadSeq
    """
    _size

  fun apply(i: USize): this->A ? =>
    """
    Get the i-th element, raising an error if the index is out of bounds.
    """
    if i < _size then
      _apply(i)
    else
      error
    end

  fun ref update(i: USize, value: A): A^ ? =>
    """
    Change the i-th element, raising an error if the index is out of bounds.
    """
    if i < _size then
      _update(i, consume value)
    else
      error
    end

  fun add[_size': USize](that: Vector[this->A, _size']):
        Vector[this->A!, #(_size + _size')]^ =>
    """
    Create a new vector with references to the contents of this
    vector and the argument vector.
    """
    let result = Vector[this->A, #(_size + _size')]._create()
    var i: USize = 0
    while i < _size do
      result._update(i, _apply(i))
      i = i + 1
    end
    i = 0
    while i < _size' do
      result._update(i + _size, that._apply(i))
      i = i + 1
    end
    result

  fun keys(): VectorKeys[A, _size, this->Vector[A, _size]]^ =>
    """
    Return an iterator over the indices in the vector.
    """
    VectorKeys[A, _size, this->Vector[A, _size]]

  fun values(): VectorValues[A, _size, this->Vector[A, _size]]^ =>
    """
    Return an iterator over the values in the vector.
    """
    VectorValues[A, _size, this->Vector[A, _size]](this)

  fun pairs(): VectorPairs[A, _size, this->Vector[A, _size]]^ =>
    """
    Return an iterator over the pairs of indices and values in the vector.
    """
    VectorPairs[A, _size, this->Vector[A, _size]](this)

class VectorKeys[A, _size: USize, B: Vector[A, _size] #read] is Iterator[USize]
  var _i: USize

  new create() =>
    _i = 0

  fun has_next(): Bool =>
    _i < _size

  fun ref next(): USize =>
    if _i < _size then
      _i = _i + 1
    else
      _i
    end

class VectorValues[A, _size: USize, B: Vector[A, _size] #read] is Iterator[B->A]
  let _vector: B
  var _i: USize

  new create(vector: B) =>
    _vector = vector
    _i = 0

  fun has_next(): Bool =>
    _i < _size

  fun ref next(): B->A ? =>
    _vector(_i = _i + 1)

class VectorPairs[A, _size: USize, B: Vector[A, _size] #read] is Iterator[(USize, B->A)]
  let _vector: B
  var _i: USize

  new create(vector: B) =>
    _vector = vector
    _i = 0

  fun has_next(): Bool =>
    _i < _size

  fun ref next(): (USize, B->A) ? =>
    (_i, _vector(_i = _i + 1))
