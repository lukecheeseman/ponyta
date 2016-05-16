interface Sortable[A: Comparable[A] #read ]
  fun size(): USize
  fun apply(i: USize): A ?
  fun ref update(i: USize, value: A): A^ ?

primitive Sort[A: Comparable[A] #read]
  fun _swap(data: Sortable[A], i: USize, j: USize) ? =>
    let tmp = data(i)
    data.update(i, data(j))
    data.update(j, tmp)

  fun reverse(data: Sortable[A]) ? =>
    var i: USize = 0
    var j: USize = data.size() - 1
    while i < j do
      _swap(data, i, j)
      i = i + 1
      j = j - 1
    end

  fun sorted(v: ReadSeq[A]): Bool ? =>
    if v.size() < 2 then
      true
    end

    let it = v.values()
    var prev = it.next()
    while it.has_next() do
      let curr = it.next()
      if prev > curr then
        return false
      end
      prev = curr
    end
    true

  fun _sub[B: Real[B] val](x: B, y: B): B =>
    if x > y then x - y else x.min_value() end

  fun _add[B: Real[B] val](x: B, y: B): B =>
    if x < (y.max_value() - y) then x + y else x.max_value() end

  fun quick_sort(data: Sortable[A]) ? =>
    _quick_sort_impl(data, 0, data.size() - 1)

  fun _quick_sort_impl(data: Sortable[A], left: USize, right: USize) ? =>
    var i: USize = left
    var j: USize = right
    let pivot: A = data(((left + right) / 2))
    while i <= j do
      while data(i) < pivot do i = _add[USize](i, 1) end
      while data(j) > pivot do j = _sub[USize](j, 1) end
      if i <= j then
        _swap(data, i, j)
        i = _add[USize](i, 1)
        j = _sub[USize](j, 1)
      end
    end

    if left < j then
      _quick_sort_impl(data, left, j)
    end

    if i < right then
      _quick_sort_impl(data, i, right)
    end

  fun insertion_sort(data: Sortable[A]) ? =>
    var i: USize = 0
    while i < data.size() do
      var elem = data(i)
      var j: USize = 0
      while j < i do
        if elem < data(j) then
          let tmp = data(j)
          data.update(j, elem)
          elem = tmp
        end
        j = j + 1
      end
      data.update(j, elem)
      i = i + 1
    end

  fun selection_sort(data: Sortable[A]) ? =>
    var head: USize = 0
    while head < data.size() do
      var min: USize = head
      var i: USize = head
      while i < data.size() do
        min = if data(i) < data(min) then i else min end
        i = i + 1
      end
      if head != min then
        _swap(data, head, min)
      end
      head = head + 1
    end

  fun _heap_filter_up(data: Sortable[A], k: USize) ? =>
    var node = k
    while node != 0 do
      let parent = (node - 1) / 2
      if data(node) < data(parent) then
        _swap(data, parent, node)
        node = parent
      else
        return
      end
    end

  fun _heap_filter_down(data: Sortable[A], num_nodes: USize) ? =>
    var node: USize = 0
    while ((2 * node) + 1) < num_nodes do
      let child1 = (2 * node) + 1
      let child2 = (2 * node) + 2

      if child2 < num_nodes then
        if (data(node) < data(child1)) and (data(node) < data(child2)) then
          return
        end

        if data(child1) < data(child2) then
          _swap(data, node, child1)
          node = child1
        else
          _swap(data, node, child2)
          node = child2
        end

      else
        if data(node) < data(child1) then
          return
        end

        _swap(data, node, child1)
        node = child1
      end
    end

  fun heap_sort(data: Sortable[A]) ? =>
    var num_nodes: USize = 0
    var i: USize = 0
    while i < data.size() do
      _heap_filter_up(data, num_nodes)
      num_nodes = num_nodes + 1
      i = i + 1
    end

    i = 0
    while i < data.size() do
      num_nodes = num_nodes - 1
      _swap(data, 0, num_nodes)
      _heap_filter_down(data, num_nodes)
      i = i + 1
    end

    reverse(data)

  fun bubble_sort(data: Sortable[A]) ? =>
    var swapped = false
    var limit: USize = data.size()
    repeat
      swapped = false
      var i: USize = 0
      while i < (limit - 1) do
        if data(i) > data(i + 1) then
          _swap(data, i, i + 1)
          swapped = true
        end
        i = i + 1
      end
      limit = limit - 1
    until not swapped end

  fun merge_sort(data: Sortable[A]) ? =>
    _merge_sort_impl(data, 0, data.size() - 1)

  fun _merge_sort_impl(data: Sortable[A], first: USize, last: USize) ? =>
    if first >= last then
      return
    end

    let mid = (first + last) / 2
    _merge_sort_impl(data, first, mid)
    _merge_sort_impl(data, mid + 1, last)

    _merge(data, first, last)

  fun _merge(data: Sortable[A], first: USize, last: USize) ? =>
    var left = first
    var mid = (first + last) / 2
    var right = mid + 1
    if data(right - 1) <= data(right) then
      return
    end

    while (left <= mid) and (right <= last) do
      if data(left) <= data(right) then
        left = left + 1
      else
        let tmp = data(right)
        _shift(data, left, right - left)
        data.update(left, tmp)
        left = left + 1
        mid = mid + 1
        right = right + 1
      end
    end

  fun _shift(data: Sortable[A], first: USize, len: USize) ? =>
    var i: USize = 0
    while i < len do
      data.update((first + 1) + i, data(first + i))
      i = i + 1
    end
