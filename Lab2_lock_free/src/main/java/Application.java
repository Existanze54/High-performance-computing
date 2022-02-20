import java.util.Iterator;

import java.util.Vector;

class Application {
    public static void main(String[] args) {
        SetImpl<Integer> set = new SetImpl<Integer>();
        System.out.println("Hello, World!");
        set.print();
        set.add(5);
        set.print();
        set.add(3);
        set.print();
        if (!set.isEmpty())
            System.out.println("Set is not empty");
        set.add(7);
        set.print();
        set.add(3);
        set.print();

        if (set.contains(3))
            System.out.println("Set contais 3");
        if (set.contains(5))
            System.out.println("Set contais 5");
        if (set.contains(7))
            System.out.println("Set contais 7");

//        for (Integer elem : set)
//            System.out.println(elem);

        Iterator<Integer> itr = set.iterator();

        // Check until iterator has not reached end
        while (itr.hasNext())
        {
            System.out.print(itr.next() + " ");
        }

        set.remove(5);
        set.print();
        set.remove(7);
        set.print();
        if (!set.contains(7))
            System.out.println("Set doesn't contain 7");
        set.remove(3);
        set.print();

        if (set.isEmpty())
            System.out.println("Set is empty");

        set.remove(3);
        set.print();
        set.add(10);
        set.print();

        Integer a = 5;
        if (null == a)
            System.out.println("null != a");

    }
}