// Run with: node tests/BankImportRememberTest.js
const path = require('path');
const project = path.resolve(__dirname, '..');
const fs = require('fs'), vm = require('vm'), assert = require('assert/strict');
const qml = fs.readFileSync(path.join(project, 'BankCsvImportDialog.qml'),'utf8');
const methods = qml.slice(qml.indexOf('    function choiceFor'),qml.indexOf('    signal importFinished'));
const items = qml.slice(qml.indexOf('    function categoryItems'),qml.indexOf('    function columnItems'));
const norm = s => s.normalize('NFKC').toLowerCase().replace(/ё/g,'е').replace(/[^\p{L}\p{N}]+/gu,' ').trim().replace(/ +/g,' ');
function setup(rows) {
 const c={controller:{normalizeBankCategoryText:norm,categories:[{value:'food',label:'Продукты',type:'expense'},{value:'other',label:'Другое',type:'expense'},{value:'salary',label:'Доход',type:'income'}]},categoryChoices:{},operationRows:rows,showReviewOnly:false};
 vm.createContext(c);vm.runInContext(methods+items,c);return c;
}
const row=(id,extra={})=>({rowKey:id,fingerprint:id,merchant:'Пятёрочка',description:'Оплата PYATEROCHKA '+id,type:'expense',categoryId:'food',needsReview:true,special:false,...extra});
let rows=Array.from({length:1000},(_,i)=>row(String(i))),c=setup(rows);
c.setChoice(rows[0],{categoryId:'other',confirmed:true});
assert.equal(c.remainingReviews(),999);
c.setChoice(rows[0],{remember:true});
assert.equal(c.remainingReviews(),0);
assert.equal(Object.values(c.categoryChoices).filter(x=>x.remember).length,1);
assert.ok(rows.every(r=>c.choiceFor(r).categoryId==='other'));
c.setChoice(rows[0],{categoryId:'food'});assert.ok(rows.every(r=>c.choiceFor(r).categoryId==='food'));
c.setChoice(rows[1],{categoryId:'other',confirmed:true});
c.setChoice(rows[0],{remember:false});
assert.equal(c.choiceFor(rows[1]).categoryId,'other');assert.equal(c.choiceFor(rows[1]).confirmed,true);
assert.equal(c.choiceFor(rows[2]).confirmed,false);assert.equal(c.remainingReviews(),998);
rows=[row('a',{merchant:'платеж 111 в platipomiru',description:'Платёж 111 в PLATIPOMIRU через СБП',special:true}),row('b',{merchant:'платеж 222 в platipomiru',description:'Платёж 222 в platipomiru через систему быстрых платежей'}),row('c',{merchant:'другой',description:'Платёж в platipomiruXXX'}),row('income',{type:'income',categoryId:'salary'}),row('d',{description:'Платёж в platipomiru2'})];c=setup(rows);
c.setChoice(rows[0],{categoryId:'other',confirmed:true,remember:true,matchMode:'contains',pattern:'PLATIPOMIRU'});
assert.equal(c.choiceFor(rows[1]).confirmed,true);assert.equal(c.choiceFor(rows[1]).categoryId,'other');
assert.equal(c.choiceFor(rows[2]).confirmed,false);assert.equal(c.choiceFor(rows[3]).confirmed,false);assert.equal(c.choiceFor(rows[4]).confirmed,false);
c.setChoice(rows[0],{pattern:'несуществующий получатель'});assert.equal(c.choiceFor(rows[1]).confirmed,false);
c.setChoice(rows[0],{pattern:''});assert.equal(c.choiceFor(rows[1]).confirmed,false);
c.setChoice(rows[0],{pattern:'platipomiru'});assert.equal(c.choiceFor(rows[1]).confirmed,true);
c.operationRows.push(row('refund',{description:'Возврат platipomiru',special:true}));
c.setChoice(rows[0],{categoryId:'food'});assert.equal(c.choiceFor(c.operationRows[5]).categoryId,'food');assert.equal(c.choiceFor(c.operationRows[5]).confirmed,false);
// Equal-specificity conflicting rules must leave unresolved rows unconfirmed.
rows=[row('1'),row('2'),row('3')];c=setup(rows);
c.setChoice(rows[0],{confirmed:true,remember:true,categoryId:'food'});
c.setChoice(rows[1],{confirmed:true,remember:true,categoryId:'other'});assert.equal(c.choiceFor(rows[2]).confirmed,false);
// Matching helpers use Qt normalization in production, including Unicode case folding.
assert.match(fs.readFileSync(path.join(project, 'interface/FinanceController.cpp'),'utf8'),/return BankCategoryMatcher::normalize\(text\);/);
console.log('Проверки пройдены: 1000 строк, точное правило, описание содержит, границы слов, изменение и снятие правила, ручные исключения, доходы, возвраты и конфликты.');
