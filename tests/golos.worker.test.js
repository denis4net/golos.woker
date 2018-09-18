const EOSTest = require('eosio.test')

it('tests hello contract', async (done) => {
  const eosTest = new EOSTest()
  const appName = 'app.sample'

  console.log('build contract')
  await eosTest.make('contracts/golos.worker')
  
  console.log('init')
  await eosTest.init()
  
  console.log('create test account')
  await eosTest.newAccount('user1')
  
  console.log("create a delgate's accounts")
  await eosTest.newAccount('delegate1')
  await eosTest.newAccount('delegate2')

  console.log('create a golos.worker account')
  await eosTest.newAccount('golos.worker')
  
  console.log('create an app.sample account')
  await eosTest.newAccount(appName)

  console.log('deploy contract')
  const contract = await eosTest.deploy('golos.worker', 'contracts/golos.worker/golos.worker.wasm', 'contracts/golos.worker/golos.worker.abi')
  
  console.log('testing contract')
  await contract.create(appName, 'sample', {authorization: appName})
  const proposalId = await contract.addpropos(appName, 'test', 'Proposal #1', "Let's create golos workre's pool", {authorization: "user1"})
  await contract.upvote(appName, proposalId, 'delegate1', {authorization: 'delegate1'})
  await contract.downvote(appName, proposalId, 'delegate2', {authorization: 'delegate2'})

  comments.push(await contract.addcomment(appName, proposalId, 'delegate1', "Let's create!", {authorization: 'delegate1'}))
  comments.push(await contract.addcomment(appName, proposalId, 'delegate2', "No.....!", {authorization: 'delegate2'}))
    
  await contract.del_proposal(appName, proposalId)

  await contract.delcomment(appName, comments[0], {authorization: 'delegate1'})
  await contract.delcomment(appName, comemnts[1], {authorization: 'delegate2'})

  await eosTest.destroy()
  done()
}, 120000)