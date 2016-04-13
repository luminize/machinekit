<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE refentry PUBLIC "-//OASIS//DTD DocBook XML V4.4//EN"
                   "http://www.oasis-open.org/docbook/xml/4.4/docbookx.dtd">
<!-- lifted from man+troff by doclifter -->
<refentry id='bitwise9icomp'>
<refentryinfo><date>2016-03-04</date></refentryinfo> <refmeta>
<refentrytitle>BITWISE</refentrytitle> <manvolnum>9</manvolnum>
<refmiscinfo class='date'>2016-03-04</refmiscinfo>
<refmiscinfo class='source'>Machinekit Documentation</refmiscinfo>
<refmiscinfo class='manual'>HAL Component</refmiscinfo> </refmeta>
<refnamediv> <refname>bitwise</refname> <refpurpose>Computes various
bitwise operations on the two input values</refpurpose> </refnamediv>
<!-- body begins here --> <refsynopsisdiv id='synopsis'>
<cmdsynopsis>
  
<command>bitwise</command>\
</cmdsynopsis>
</refsynopsisdiv>

<refsect1 id='instantiable_components'>
<title>
INSTANTIABLE COMPONENTS
</title>
<para>
<emphasis role='strong' remap='B'>All instantiable components can be
loaded in two manners</emphasis>

<emphasis role='strong' remap='B'>Using loadrt with or without count= |
names= parameters as per legacy components</emphasis>

<emphasis role='strong' remap='B'>Using newinst, which names the
instance and allows further parameters and arguments,</emphasis>

<emphasis role='strong' remap='B'>primarily pincount= which can set the
number of pins created for that instance (where applicable)</emphasis>
</para>

</refsect1>

<refsect1 id='usage_synopsis'>
<title>
USAGE SYNOPSIS
</title>


<para>
<emphasis role='strong' remap='B'>loadrt bitwise</emphasis>
</para>

<para>
<emphasis role='strong' remap='B'>newinst bitwise \<newinstname\> [
pincount=</emphasis><emphasis remap='I'>N</emphasis><emphasis role='strong' remap='B'>
|
iprefix=</emphasis><emphasis remap='I'>prefix</emphasis><emphasis role='strong' remap='B'>
]</emphasis>
<emphasis role='strong' remap='B'>[instanceparamX=</emphasis><emphasis remap='I'>X</emphasis><emphasis role='strong' remap='B'>
|
argX=</emphasis><emphasis remap='I'>X</emphasis><emphasis role='strong' remap='B'>
]</emphasis>
</para>


</refsect1>

<refsect1 id='functions'>
<title>
FUNCTIONS
</title>


<variablelist remap='TP'>
  
<varlistentry>
<term><emphasis role='strong' remap='B'>bitwise.N.funct</emphasis></term>
<listitem>
<para>
( OR <emphasis role='strong' remap='B'>\<newinstname\>.funct</emphasis>
)
</para>
  
</listitem> </varlistentry>
</variablelist>


</refsect1>

<refsect1 id='pins'>
<title>
PINS
</title>


<variablelist remap='TP'>
  
<varlistentry> <term><emphasis role='strong' remap='B'>bitwise.N.in0
</emphasis> u32 in </term> <listitem>
<para>
( OR <emphasis role='strong' remap='B'>\<newinstname\>.in0 </emphasis>
u32 in )
</para>

</listitem> </varlistentry>
</variablelist>

<para>
First input value
</para>
<variablelist remap='TP'>
  
<varlistentry> <term><emphasis role='strong' remap='B'>bitwise.N.in1
</emphasis> u32 in </term> <listitem>
<para>
( OR <emphasis role='strong' remap='B'>\<newinstname\>.in1 </emphasis>
u32 in )
</para>

</listitem> </varlistentry>
</variablelist>

<para>
Second input value
</para>
<variablelist remap='TP'>
  
<varlistentry> <term><emphasis role='strong' remap='B'>bitwise.N.out-and
</emphasis> u32 out </term> <listitem>
<para>
( OR <emphasis role='strong' remap='B'>\<newinstname\>.out-and
</emphasis> u32 out )
</para>

</listitem> </varlistentry>
</variablelist>

<para>
The bitwise AND of the two inputs
</para>
<variablelist remap='TP'>
  
<varlistentry> <term><emphasis role='strong' remap='B'>bitwise.N.out-or
</emphasis> u32 out </term> <listitem>
<para>
( OR <emphasis role='strong' remap='B'>\<newinstname\>.out-or
</emphasis> u32 out )
</para>

</listitem> </varlistentry>
</variablelist>

<para>
The bitwise OR of the two inputs
</para>
<variablelist remap='TP'>
  
<varlistentry> <term><emphasis role='strong' remap='B'>bitwise.N.out-xor
</emphasis> u32 out </term> <listitem>
<para>
( OR <emphasis role='strong' remap='B'>\<newinstname\>.out-xor
</emphasis> u32 out )
</para>

</listitem> </varlistentry>
</variablelist>

<para>
The bitwise XOR of the two inputs
</para>
<variablelist remap='TP'>
  
<varlistentry>
<term><emphasis role='strong' remap='B'>bitwise.N.out-nand </emphasis>
u32 out </term> <listitem>
<para>
( OR <emphasis role='strong' remap='B'>\<newinstname\>.out-nand
</emphasis> u32 out )
</para>

</listitem> </varlistentry>
</variablelist>

<para>
The inverse of the bitwise AND
</para>
<variablelist remap='TP'>
  
<varlistentry> <term><emphasis role='strong' remap='B'>bitwise.N.out-nor
</emphasis> u32 out </term> <listitem>
<para>
( OR <emphasis role='strong' remap='B'>\<newinstname\>.out-nor
</emphasis> u32 out )
</para>

</listitem> </varlistentry>
</variablelist>

<para>
The inverse of the bitwise OR
</para>
<variablelist remap='TP'>
  
<varlistentry>
<term><emphasis role='strong' remap='B'>bitwise.N.out-xnor </emphasis>
u32 out </term> <listitem>
<para>
( OR <emphasis role='strong' remap='B'>\<newinstname\>.out-xnor
</emphasis> u32 out )
</para>

</listitem> </varlistentry>
</variablelist>

<para>
The inverse of the bitwise XOR
</para>
</refsect1>

<refsect1 id='author'>
<title>
AUTHOR
</title>


<para>
Andy Pugh
</para>
</refsect1>

<refsect1 id='license'>
<title>
LICENSE
</title>


<para>
GPL 2+
</para>
</refsect1> </refentry>