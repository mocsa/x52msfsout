If you call

```c++
xml_file.get_child("assignments")
```

on this XML

```xml
<assignments clear_all="false">
    <button nr="19" dataref="ELEVATOR TRIM POSITION%Radians" type="trigger_pos" on="0" />
    <button nr="18" command="ELEV_TRIM_UP" type="trigger_pos" >
    </button>
    <button nr="17" command="ELEV_TRIM_DOWN" type="trigger_pos" />
</assignments>
```

then it will be translated into a ptree looking like this

```json
<xmlattr>
{
    clear_all false
}
button  ;'button' key (no data inside the opening and closing tag)
{
    <xmlattr>	;special <xmlattr> subkey (no data)
    {
        nr 19   ;'nr' attribute and its value
        dataref "ELEVATOR TRIM POSITION%Radians"
        type "trigger_pos"
        on = "0"
    }
}
button
{
    <xmlattr>
    {
        nr 18
        dataref "ELEV_TRIM_UP"
        type "trigger_pos"
    }
}
button
{
    <xmlattr>
    {
        nr 17
        dataref "ELEV_TRIM_DOWN"
        type "trigger_pos"
    }
}
```

Write XML file starting from a value_type of only attributes by reconstructing the element.

```c++
boost::property_tree::ptree pt;
pt.add_child("button", xmltree.second);
boost::property_tree::write_xml("testXml.xml", pt, std::locale());
```

ptree::value_type is defined like `typedef std::pair< const Key, self_type> value_type;` so it is essentially a std::pair. Source: [How do we get objects in boost property tree](https://stackoverflow.com/questions/11902065/how-do-we-get-objects-in-boost-property-tree)

value_type.first is the name of the tag

value_type.second is the ptree below this element, the children of this element ([StackOverflow Boost Property_Tree iterators, how to handle them?](https://stackoverflow.com/a/4775690))

[StackOverflow How to iterate a boost property tree?](https://stackoverflow.com/questions/4586768/how-to-iterate-a-boost-property-tree) Mentions `for (auto& it: tree)` at the end since C++11. Also mentions `ptree::const_iterator`.

[Boost 1.46.1, Property Tree: How to iterate through ptree receiving sub ptrees?](https://stackoverflow.com/questions/6656380/boost-1-46-1-property-tree-how-to-iterate-through-ptree-receiving-sub-ptrees) says „With C++17 you can do it even more elegant with structured bindings: `for (const auto& [key, value] : children) { ... }`”

[C++ Cheatsheets](https://www.codecademy.com/resources/cheatsheets/language/c-plus-plus)

Older but a bit better doc of ptree: http://kaalus.atspace.com/ptree/doc/index.html