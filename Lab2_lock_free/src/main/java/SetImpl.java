import java.util.Iterator;

import java.util.concurrent.atomic.*;
import java.util.Vector;

class Node<T> {
    T value;
    AtomicMarkableReference<Node<T>> next;

    public Node(T value, Node<T> next) {
        this.value = value;
        this.next = new AtomicMarkableReference<Node<T>>(next, false);
    }
}

public class SetImpl<T extends Comparable<T>> implements Set<T> {
    // ИНВАРИАНТ: node.key < node.next.key

    // Пустой список состоит из 2-х граничных элементов
    // Объединим next и marked в одну переменную{next,marked}, которую будем атомарно менять используя CAS
    Node<T> headNode;
    AtomicMarkableReference<Node<T>> head;
    Node<T> tailNode;
    AtomicMarkableReference<Node<T>> tail;

    public SetImpl() {
        this.headNode = new Node<T>(null, null);
        this.tailNode = new Node<T>(null, null);

        this.head = new AtomicMarkableReference<Node<T>>(tailNode, false);
        this.tail = new AtomicMarkableReference<Node<T>>(null, false);

        headNode.next = head;
        tailNode.next = tail;
    }

    public class FindResult {
        Node first;
        Node second;

        public FindResult(Node first, Node second) {
            this.first = first;
            this.second = second;
        }
    }

    // Возвращает пару (pred, curr) по ключу: pred.value < value <= curr.value
    public FindResult find(T value){
        retry: while(true) {
            Node<T> pred = this.headNode, curr = pred.next.getReference(), succ;
            while (true) {
                succ = curr.next.getReference();
                boolean cmk = curr.next.isMarked();
                if (cmk) { // Если curr логически удален
                    // Помогаем другим потокам прокидывать указатель, wait-free
                    if (!pred.next.compareAndSet(curr, succ, false, false))
                        continue retry;
                    curr = succ;
                }
                else {
                    if (curr == tailNode) // При пустом списке
                        return new FindResult(pred, curr);

                    if (curr.value.compareTo(value) >= 0)
                        return new FindResult(pred, curr);
                    pred = curr;
                    curr = succ;
                }
            }
        }
    }

    @Override
    public boolean add(T value) {
        while (true) {
            Node<T> pred, curr;
            FindResult res = find(value);
            pred = res.first;
            curr = res.second;

            if (curr.value == value)
                return false;
            else {
                Node<T> node = new Node(value, curr);
                if (pred.next.compareAndSet(curr, node, false, false))
                    return true;
            }
        }
    }

    @Override
    public boolean remove(T value) {
        while (true) {
            Node<T> pred, curr;
            FindResult res = find(value);
            pred = res.first;
            curr = res.second;

            if (curr.value != value)
                return false;
            else {
                Node succ = curr.next.getReference();
                if (!curr.next.compareAndSet(succ, succ, false, true)) // логически удаляем
                    continue;

                // оптимизация – попытаемся физ. удалить
                pred.next.compareAndSet(curr, succ, false, false);
                return true;
            }
        }
    }

    public boolean contains(T value){
        retry: while(true) {
            Node<T> pred = this.headNode, curr = pred.next.getReference(), succ;
            while (true) {
                succ = curr.next.getReference();
                boolean cmk = curr.next.isMarked();
                if (cmk) { // Если curr логически удален
                    // Помогаем другим потокам прокидывать указатель, wait-free
                    if (!pred.next.compareAndSet(curr, succ, false, false))
                        continue retry;
                    curr = succ;
                }
                else {
                    if (curr == tailNode) // При пустом списке
                        return false;

                    if (curr.value.compareTo(value) == 0)
                        return true;
                    pred = curr;
                    curr = succ;
                }
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return (this.headNode.next.getReference() == this.tailNode);
    }

    public void print() {
        System.out.printf("Values: ");
        for (Node<T> node = this.headNode.next.getReference(); node != this.tailNode; node = node.next.getReference()) {
            System.out.printf(" " + node.value.toString());
        }
        System.out.println();
    }

    @Override
    public Iterator<T> iterator() {
        Vector<Node<T>> snapshot_1 = new Vector(), snapshot_2 = new Vector(), agreedSnapshot = new Vector();

        while (true){
            snapshot_1.clear();
            snapshot_2.clear();

            for (Node<T> node = headNode.next.getReference(); node != tailNode; node = node.next.getReference()) {
                snapshot_1.addElement(node);
            }

            for (Node<T> node = headNode.next.getReference(); node != tailNode; node = node.next.getReference()) {
                snapshot_2.addElement(node);
            }

            if (snapshot_1.equals(snapshot_2)) {
                agreedSnapshot.clear();
                for (Node<T> node : snapshot_2)
                    if (!node.next.isMarked())
                        agreedSnapshot.addElement(node);
                break;
            }
        }

        Iterator<T> it = new Iterator<T>() {
            private Integer currentIndex = 0;

            @Override
            public boolean hasNext() {
                return currentIndex < agreedSnapshot.size();
            }

            @Override
            public T next() {
                return agreedSnapshot.get(currentIndex++).value;
            }
        };
        return it;
    }
}

